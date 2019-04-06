#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <9pclient.h>
#include <acme.h>

/* main window */

enum
{
	Cdelete = 0,
	Cstatus,
	Clog,
	Ccommit,
	Cpush
};

extern Win *win;

void	mkwin(void);
int		getcmd(Event *e);
void	eventloop(void);

/* git common */

int		gitfd(char *argv[]);
void 	gitpipetowin(char *argv[]);
void 	gitstatus(void);
void	gitlog(void);
void	gitcommit(void);
void	gitpush(void);

/* git status specific */

typedef struct Status Status;

enum
{
	Staged = 0,
	Modified,
	Renamed,
	Untracked
};

char *statestr[] = {
	"Staged",
	"Modified",
	"Renamed",
	"Untracked"
};

struct Status
{
	int 	state;
	char	*filename;
	char	*extra;
	Status	*next;
};

Status*	mkstatus(int s, char *f, char *e);
void	deletestatus(Status *s);
Status*	parsestatus(char *s);
void	printstatus(Status *s);

