#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <9pclient.h>
#include <acme.h>

typedef struct Status Status;

struct Status
{
	int 	state;
	char	*filename;
	char	*extra;
	Status	*next;
};

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

char *gitlog[] = { "git", "log", nil };
char *gitcommit[] = { "git", "commit", nil };
char *gitpush[] = { "git", "push", "--porcelain", nil };

Win* win;

Status*
mkstatus(int s, char *f, char *e)
{
	Status *status;

	status = emalloc(sizeof(Status));
	status->state = s;
	status->filename = f;
	status->extra = e;
	status->next = nil;
	return status;
}

Status*
parsestatus(char* line)
{
	Status* status;
	char* tokens[2];
	char* filename;

	status = nil;
	tokenize(line, tokens, 2);
	filename = tokens[1];
	switch(line[0]){
	case ' ':
	case 'R':
		if(line[1] == 'M'){
			status = mkstatus(Modified, filename, nil);
		} else {
			fprint(2, "unknown status '%c%c'", line[0], line[1]);
		}
		break;
	case 'M':
	case 'A':
		status = mkstatus(Staged, filename, nil);
		break;
	case '?':
		status = mkstatus(Untracked, filename, nil);
		break;
	}
	return status;
}

void
printstatus(Status *status)
{
	int s;

	s = -1;
	winprint(win, "addr", "1,$");
	if(status == nil)
		winprint(win, "data", "Everything is up to date\n");
	for(; status; status = status->next){
		if(status->state != s){
			if(s != -1)
				winprint(win, "data", "\n");
			winprint(win, "data", "%s files:\n", statestr[status->state]);
			s = status->state;
		}
		switch(status->state){
		case Staged:
			winprint(win, "data", "\t[git reset HEAD %s]\n", status->filename);
			break;
		case Modified:
			winprint(win, "data", "\t[git add %s] [git diff %s]\n", status->filename, status->filename);
			break;
		case Renamed:
			break;
		case Untracked:
			winprint(win, "data", "\t[git add %s]\n", status->filename);
			break;
		}
	}
	winprint(win, "ctl", "clean\n");
}

int
git(char** argv)
{
	int p[2], pid;

	if(pipe(p) < 0)
		sysfatal("pipe: %r");
	pid = fork();
	if(pid < 0)
		sysfatal("fork: %r");
	if(pid == 0){
		close(p[0]);
		dup(p[1], 1);
		exec("git", argv);
		sysfatal("exec: %r");
	}
	close(p[1]);
	return p[0];
}

void
gitstatus(void)
{
	char* argv[4] = { "git", "status", "--porcelain", nil };
	int fd;
	Biobuf bio;
	char *line;
	Status *status, *current, *s;

	status = nil;
	fd = git(argv);
	Binit(&bio, fd, OREAD);
	while((line = Brdstr(&bio, '\n', 0)) != nil){
		s = parsestatus(line);
		if(status){
			current->next = s;
			current = current->next;
		}else{
			status = s;
			current = status;
		}
	}
	close(fd);
	Bterm(&bio);
	printstatus(status);
}

void
gitcmd(char **argv)
{
	int fd;
	Biobuf bio;
	char *line;

	fd = git(argv);
	Binit(&bio, fd, OREAD);
	winprint(win, "addr", "1,$");
	while((line = Brdstr(&bio, '\n', 0)) != nil){
		winprint(win, "data", line);
	}
	winprint(win, "ctl", "clean\n");
	close(fd);
	Bterm(&bio);
}

void
cd(char* dir)
{
	int fd;

	fd = open(dir, O_RDONLY);
	if(fd < 0)
		sysfatal("open: %r");
	if(fchdir(fd) < 0)
		sysfatal("fchdir: %r");
	close(fd);
}

void
threadmain(int argc, char** argv)
{
	char* revparseargs[4] = { "git", "rev-parse", "--show-toplevel", nil };
	char pwd[1024];
	int fd;
	Biobuf bio;
	char *line;
	Channel* c;
	int handled;
	Event *e;

	fd = git(revparseargs);
	Binit(&bio, fd, OREAD);
	line = Brdstr(&bio, '\n', 0);
	if(line == nil)
		sysfatal("unable to get toplevel dir");
	line[strlen(line)-1] = '\0';
	cd(line);
	close(fd);
	Bterm(&bio);
	win = newwin();
	winprint(win, "ctl", "cleartag\n");
	winprint(win, "tag", " Log Status Commit Push");
	winname(win, "%s/+Git", getwd(pwd, 1024));
	gitstatus();
	c = wineventchan(win);
	for(;;){
		handled = 1;
		e = recvp(c);
		if(e->c1 == 'M' && (e->c2 == 'x' || e->c2 == 'X')){
			if(strcmp(e->text, "Del") == 0){
				winwriteevent(win, e);
				break;
			}else if(strcmp(e->text, "Status") == 0){
				gitstatus();
			}else if(strcmp(e->text, "Log") == 0){
				gitcmd(gitlog);
			}else if(strcmp(e->text, "Commit") == 0){
				gitcmd(gitcommit);
			}else if(strcmp(e->text, "Push") == 0){
				gitcmd(gitpush);
			}else{
				handled = 0;
			}
		}
		if(!handled){
			winwriteevent(win, e);
		}
	}
	threadexitsall(nil);
}
