#include "a.h"

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

void
deletestatus(Status *s)
{
	Status *c, *n;

	for(c = s; c;){
		n = c->next;
		if(c->filename)
			free(c->filename);
		if(c->extra)
			free(c->extra);
		free(c);
		c = n;		
	}
}

Status*
parsestatus(char *s)
{
	Status* status;
	char* tokens[2];
	char* filename;

	status = nil;
	tokenize(s, tokens, 2);
	filename = strdup(tokens[1]);
	switch(s[0]){
	case ' ':
	case 'R':
		if(s[1] == 'M'){
			status = mkstatus(Modified, filename, nil);
		} else {
			fprint(2, "unknown status '%c%c'", s[0], s[1]);
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
gitfd(char *argv[])
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
	Status *status, *c, *n;

	status = nil;
	fd = gitfd(argv);
	Binit(&bio, fd, OREAD);
	while((line = Brdstr(&bio, '\n', 0)) != nil){
		n = parsestatus(line);
		if(status){
			c->next = n;
			c = c->next;
		}else{
			status = n;
			c = status;
		}
		free(line);
	}
	close(fd);
	Bterm(&bio);
	printstatus(status);
	deletestatus(status);
}

void
gitpipetowin(char *argv[])
{
	int fd;
	Biobuf bio;
	char *line;

	fd = gitfd(argv);
	Binit(&bio, fd, OREAD);
	winprint(win, "addr", "1,$");
	while((line = Brdstr(&bio, '\n', 0)) != nil){
		winprint(win, "data", line);
		free(line);
	}
	winprint(win, "ctl", "clean\n");
	close(fd);
	Bterm(&bio);
}

void
gitlog(void)
{
	char *argv[] = { "git", "log", nil };
	gitpipetowin(argv);
}

void
gitcommit(void)
{
	char *argv[] = { "git", "commit", nil };
	gitpipetowin(argv);
}

void
gitpush(void)
{
	char *argv[] = { "git", "push", "--porcelain", nil };
	gitpipetowin(argv);
}


void
gitcdroot(void)
{
	char *argv[4] = { "git", "rev-parse", "--show-toplevel", nil };
	Biobuf bio;
	char *s;
	int fd;

	fd = gitfd(argv);
	Binit(&bio, fd, OREAD);
	s = Brdstr(&bio, '\n', 1);
	if(s == nil)
		sysfatal("unable to get toplevel dir");
	close(fd);
	Bterm(&bio);
	fd = open(s, O_RDONLY);
	if(fd < 0){
		free(s);
		sysfatal("open: %r");
	}
	if(fchdir(fd) < 0){
		close(fd);
		free(s);
		sysfatal("fchdir: %r");
	}
	close(fd);
	free(s);
}

void
mkwin(void)
{
	char pwd[1024];
	
	win = newwin();
	winprint(win, "ctl", "cleartag\n");
	winprint(win, "tag", " Log Status Commit Push");
	winname(win, "%s/+Git", getwd(pwd, 1024));
}

int
getcmd(Event *e)
{
	int cmd;

	cmd = -1;
	if(e->c1 == 'M' && (e->c2 == 'x' || e->c2 == 'X')){
		if(!strcmp(e->text, "Del"))
			cmd = Cdelete;
		else if(!strcmp(e->text, "Status"))
			cmd = Cstatus;
		else if(!strcmp(e->text, "Log"))
			cmd = Clog;
		else if(!strcmp(e->text, "Commit"))
			cmd = Ccommit;
		else if(!strcmp(e->text, "Push"))
			cmd = Cpush;
	}
	return cmd;
}

void
eventloop(void)
{
	Channel* c;
	Event *e;
	int cmd;

	c = wineventchan(win);
	for(;;){
		e = recvp(c);
		cmd = getcmd(e);
		switch(cmd){
		case Cdelete:
			winwriteevent(win, e);
			threadexitsall(nil);
			break;
		case Cstatus:
			gitstatus();
			break;
		case Clog:
			gitlog();
			break;
		case Ccommit:
			gitcommit();
			break;	
		case Cpush:
			gitpush();
			break;
		default:
			winwriteevent(win, e);
			break;
		}
	}
}

void
threadmain(int argc, char** argv)
{
	gitcdroot();
	mkwin();
	gitstatus();
	eventloop();
}
