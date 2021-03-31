#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define USAGE_MSG "Usage: %s <directory name> [-name <pattern>] [-type <f | d>] [-follow] [-xdev]\n"

#define HELP_MSG "Recursively print files in directory <directory name>.\n\n" \
                 "  -name <pattern>  only consider files matching <pattern>\n" \
                 "  -type <f|d>      only consider regular files (f) / directories (d)\n" \
                 "  -follow          follow symbolic links\n" \
                 "  -xdev            do not cross file system boundaries\n"

#define INO_HASH_SZ 100

static char *prog_name;

static void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, "Try '%s --help' for more information.\n", prog_name);
  else
    {
      printf (USAGE_MSG, prog_name);
      printf (HELP_MSG);
    }
}

// === Inode graph functions ===================================================

static struct ino_node
{
  dev_t dev;
  ino_t ino;
  char *path;

  int n_children;
  struct ino_node **children;

  struct ino_node *next;

} *ino_hash[INO_HASH_SZ];

static void
ino_hash_alloc (void)
{
  for (int i = 0; i < INO_HASH_SZ; ++i)
    ino_hash[i] = NULL;
}

static void
ino_hash_free (void)
{
  for (int i = 0; i < INO_HASH_SZ; ++i)
    {
      struct ino_node *head, *last;

      head = ino_hash[i];

      while (head)
        {
          last = head;
          head = head->next;

          free (last->path);
          free (last->children);
          free (last);
        }
    }
}

static int
label_ino_node (struct ino_node *node, char *path)
{
  node->path = malloc (strlen (path) + 1);
  if (!node->path)
    return -1;

  strcpy (node->path, path);

  return 0;
}

static struct ino_node *
create_ino_node (dev_t dev, ino_t ino)
{
  ino_t idx = ino % INO_HASH_SZ;

  struct ino_node *tmp = malloc (sizeof (*tmp));
  if (!tmp)
    {
      fprintf (stderr, "%s: %s\n", prog_name, strerror (errno));
      return NULL;
    }

  tmp->dev = dev;
  tmp->ino = ino;

  tmp->n_children = 0;
  tmp->children = NULL;

  if (ino_hash[idx])
    tmp->next = ino_hash[idx];
  else
    tmp->next = NULL;

  ino_hash[idx] = tmp;

  return tmp;
}

static struct ino_node *
get_ino_node (dev_t dev, ino_t ino)
{
  ino_t idx = ino % INO_HASH_SZ;

  struct ino_node *head = ino_hash[idx];
  while (head)
    {
      if (head->dev == dev && head->ino == ino)
        return head;

      head = head->next;
    }

  return NULL;
}

static int
extend_graph (dev_t source_dev, ino_t source_ino,
              dev_t target_dev, ino_t target_ino, char *target_path)
{
  struct ino_node *source_node = get_ino_node (source_dev, source_ino);
  if (!source_node)
    {
      source_node = create_ino_node (source_dev, source_ino);
      if (!source_node)
        return -1;
    }

  struct ino_node *target_node = get_ino_node (target_dev, target_ino);
  if (!target_node)
    {
      target_node = create_ino_node (target_dev, target_ino);
      if (!target_node)
        return -1;

      label_ino_node (target_node, target_path);
    }

  for (int i = 0; i < source_node->n_children; ++i)
    {
      if (source_node->children[i]->dev == target_dev
          && source_node->children[i]->ino == target_ino)
        {
          return 0;
        }
    }

  ++source_node->n_children;

  struct ino_node **tmp = realloc (
    source_node->children,
    source_node->n_children * sizeof (struct ino_node **));

  if (!tmp)
    {
      fprintf (stderr, "%s: %s\n", prog_name, strerror (errno));
      return -1;
    }

  source_node->children = tmp;
  source_node->children[source_node->n_children - 1] = target_node;

  return 0;
}

static int
_is_parent (struct ino_node *head, dev_t dev, ino_t ino)
{
  if (head->dev == dev && head->ino == ino)
    return 1;

  for (int i = 0; i < head->n_children; ++i)
    {
      if (_is_parent (head->children[i], dev, ino))
        return 1;
    }

  return 0;
}

static int
is_parent (dev_t head_dev, ino_t head_ino, dev_t dev, ino_t ino)
{
  ino_t idx = head_ino % INO_HASH_SZ;

  struct ino_node *head_node = ino_hash[idx];
  if (!head_node)
    return 0;

  while (head_node)
    {
      if (head_node->dev == head_dev && head_node->ino == head_ino)
        return _is_parent (head_node, dev, ino);

      head_node = head_node->next;
    }

  return 0;
}

// === Other utility functions =================================================

static int
matches (char *pattern, char *file)
{
  int match = 0;
  if (pattern)
    {
      switch (fnmatch (pattern, file, 0))
        {
        case 0:
          match = 1;
          break;
        case FNM_NOMATCH:
          break;
        default:
          fprintf (stderr, "%s: fnmatch failed\n", prog_name);
          exit (EXIT_FAILURE);
        }
    }
  else
    match = 1;

  return match;
}

static int
ignore (char *file, struct stat *sb, struct stat *lsb,
        char *pattern, int f, int d, int follow)
{
  if (!matches (pattern, file))
    return 1;

  if (f == 1 && d == 0)
    {
      if (!S_ISREG (lsb->st_mode))
        {
          if (!follow)
            return 1;
          else
            {
              if (!S_ISLNK (lsb->st_mode) || !(sb && S_ISREG (sb->st_mode)))
                return 1;
            }
        }
    }
  else if (f == 0 && d == 1)
    {
      if (!S_ISDIR (lsb->st_mode))
        {
          if (!follow)
            return 1;
          else
            {
              if (!S_ISLNK (lsb->st_mode) || !(sb && S_ISDIR (sb->st_mode)))
                return 1;
            }
        }
    }

  return 0;
}

// === Main find function ======================================================

static int
find (char *file, char *path, char *pattern, int f, int d, int follow,
      int xdev, dev_t dev, dev_t parent_dev, ino_t parent_ino, int parent_fd)
{
  // stat current file
  struct stat sb, lsb;
  struct stat *sb_ptr = NULL;

  if (parent_fd == AT_FDCWD)
    {
      if (lstat (file, &lsb) == -1)
        return 0;

      if (stat (file, &sb) != -1)
        sb_ptr = &sb;
    }
  else
    {
      if (fstatat (parent_fd, file, &lsb, AT_SYMLINK_NOFOLLOW) == -1)
        return 0;

      if (fstatat (parent_fd, file, &sb, 0) != -1)
        sb_ptr = &sb;
    }

  // check for file system loops
  if (follow && sb_ptr)
    {
      if (parent_ino != 0)
        {
          if (is_parent (sb.st_dev, sb.st_ino, parent_dev, parent_ino))
            {
              struct ino_node *parent_node = get_ino_node (parent_dev,
                                                           parent_ino);

              fprintf (stderr,
                       "%s: File system loop detected; "
                       "‘%s’ is part of the same file system loop as ‘%s’.\n",
                       prog_name, path, parent_node->path);

              return 0;
            }

          int err = extend_graph (parent_dev, parent_ino,
                                  sb.st_dev, sb.st_ino, path);
          if (err != 0)
            return 1;
        }
      else
        {
          struct ino_node *root = create_ino_node (sb.st_dev, sb.st_ino);
          label_ino_node (root, path);
        }
    }

  // print name of matching files
  if (!ignore (file, sb_ptr, &lsb, pattern, f, d, follow))
    printf ("%s\n", path);

  // stop recursion for non-directories
  if (!follow)
    {
      if (!S_ISDIR (lsb.st_mode))
        return 0;
    }
  else
    {
      if (!S_ISDIR (lsb.st_mode)
          && !(S_ISLNK (lsb.st_mode) && sb_ptr && S_ISDIR (sb.st_mode)))
        return 0;
    }

  // stop at file system boundaries when -xdev is set
  if (xdev && sb.st_dev != dev)
    return 0;

  // obtain directory fp (for calls to fstatat)
  int dirfd;
  if (parent_fd == AT_FDCWD)
    {
      if ((dirfd = open (path, O_RDONLY)) == -1)
        return 0;
    }
  else
    {
      if ((dirfd = openat (parent_fd, file, O_RDONLY)) == -1)
        return 0;
    }

  // read directory
  DIR *dird = fdopendir (dirfd);
  if (!dird)
    {
      fprintf (stderr, "%s: %s\n", prog_name, strerror (errno));
      return 1;
    }

  // iterate through directory entries
  int err = 0;

  for (;;)
    {
      errno = 0;
      struct dirent *dirent = readdir (dird);

      if (!dirent)
        {
          closedir (dird);

          if (errno == 0)
            return 0;

          fprintf (stderr, "%s: %s\n", prog_name, strerror (errno));
          return 1;
        }

      // ignore . and ..
      int is_dot = (strcmp (dirent->d_name, ".") == 0);
      int is_dotdot = (strcmp (dirent->d_name, "..") == 0);

      if (is_dot || is_dotdot)
        continue;

      // extend pathname
      char *next_path = malloc(strlen(path) + strlen(dirent->d_name) + 2);
      if (!next_path)
        {
          fprintf (stderr, "%s: %s\n", prog_name, strerror (errno));
          return 1;
        }

      sprintf (next_path, "%s/%s", path, dirent->d_name);

      // recurse
      err |= find (dirent->d_name, next_path, pattern, f, d, follow,
                   xdev, dev, sb.st_dev, sb.st_ino, dirfd);

      // free resources
      free (next_path);

      if (err == 1)
        {
          close (dirfd);
          closedir (dird);
          return 1;
        }
    }

  close (dirfd);
  closedir (dird);
  return 0;
}

// === Main ====================================================================

int
main (int argc, char **argv)
{
  // remember program name
  prog_name = argv[0];

  // parse arguments
  static struct option const long_options[] =
  {
    {"follow", no_argument, NULL, 'f'},
    {"help", no_argument, NULL, 'h'},
    {"name", required_argument, NULL, 'n'},
    {"type", required_argument, NULL, 't'},
    {"xdev", no_argument, NULL, 'x'},
    {NULL, 0, NULL, 0}
  };

  char *pattern;
  int f, d, follow, xdev;

  pattern = NULL;
  f = d = 1;
  follow = xdev = 0;

  int c;
  while ((c = getopt_long_only (argc, argv, "h", long_options, NULL)) != -1)
    {
      switch (c)
        {
        case 'f':
          follow = 1;
          break;
        case 'h':
          usage (EXIT_SUCCESS);
          free (pattern);
          exit (EXIT_SUCCESS);
        case 'n':
          pattern = malloc (strlen (optarg) + 1);
          strcpy (pattern, optarg);
          break;
        case 't':
          if (strcmp (optarg, "f") == 0)
            d = 0;
          else if (strcmp (optarg, "d") == 0)
            f = 0;
          else
            {
              fprintf (stderr, "%s: argument to 'type' should be 'f' or 'd'\n",
                       prog_name);
              free (pattern);
              exit (EXIT_FAILURE);
            }
          break;
        case 'x':
          xdev = 1;
          break;
        default:
          free (pattern);
          exit (EXIT_FAILURE);
        }
    }

  if (!argv[optind] || argv[optind + 1])
    {
      fprintf (stderr, "%s: should receive single mandatory directory argument\n",
               prog_name);

      usage (EXIT_FAILURE);
      exit (EXIT_FAILURE);
    }

  // pre-process file argument
  char *file = argv[optind];

  if (file[strlen (file) - 1] == '/')
    file[strlen (file) - 1] = '\0';

  // determine initial device
  struct stat sb;
  if (stat (file, &sb) == -1)
    {
      fprintf (stderr, "%s: %s\n", prog_name, strerror (errno));
      free (pattern);
      exit (EXIT_FAILURE);
    }

  dev_t dev = sb.st_dev;

  // allocate inode hash table / graph
  ino_hash_alloc ();

  // perform find
  int err = find (file, file, pattern, f, d, follow, xdev, dev, 0, 0, AT_FDCWD);

  // free resources and exit
  free (pattern);
  ino_hash_free ();

  if (err == 0)
    exit (EXIT_SUCCESS);
  else
    exit (EXIT_FAILURE);
}
