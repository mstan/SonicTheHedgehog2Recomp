#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "sonic2_music.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <wchar.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifndef S3_CD_IMPORT_SOURCE
#define S3_CD_IMPORT_SOURCE "tools/import_sonic_cd.py"
#endif
#ifndef S3_CD_IMPORT_VALIDATOR
#define S3_CD_IMPORT_VALIDATOR "scd_pack_validate"
#endif
#ifndef S3_ROM_IMPORT_SOURCE
#define S3_ROM_IMPORT_SOURCE "tools/import_sound_donor.py"
#endif

static int exists(const char *path) { FILE *f=fopen(path,"rb");if(!f)return 0;fclose(f);return 1; }
static int join(char *out,size_t n,const char *root,const char *name)
{ int count=snprintf(out,n,"%s%s%s",root,*root?"/":"",name);return count>=0&&(size_t)count<n; }
static void parent(char *out,size_t n,const char *path)
{
    if(out!=path)snprintf(out,n,"%s",path);
    char *a=strrchr(out,'/'),*b=strrchr(out,'\\');
    char *end=a&&(!b||a>b)?a:b;if(end==out)end[1]=0;else if(end)*end=0;else strcpy(out,".");
}
#ifdef _WIN32
static int wide(const char *in,wchar_t *out,int count)
{ return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,in,-1,out,count)!=0; }
/* Windows CRT argv quoting: backslashes before a quote or the closing quote
 * must be doubled. User paths are arguments; no command shell is executed. */
static int argument(wchar_t *out,size_t cap,size_t *used,const wchar_t *in)
{
    size_t slashes=0;
#define APPEND(ch) do{if(*used+1>=cap)return 0;out[(*used)++]=(ch);}while(0)
    APPEND(L'"');
    for(;;in++) {
        if(*in==L'\\'){slashes++;continue;}
        size_t count=slashes*((*in==L'"'||!*in)?2:1);
        while(count--){APPEND(L'\\');}slashes=0;
        if(!*in)break;
        if(*in==L'"'){APPEND(L'\\');}APPEND(*in);
    }
    APPEND(L'"');APPEND(L' ');out[*used]=0;
#undef APPEND
    return 1;
}
#endif

static void output_line(char *line,char detail[256],
                        void (*progress)(void *,float,const char *),void *user)
{
    line[strcspn(line,"\r\n")]=0;
    if(!strncmp(line,"S3K_CD_PROGRESS:",16)) {
        char *message=strchr(line+16,':');
        if(message&&progress){*message++=0;progress(user,(float)atof(line+16),message);}
    } else if(!strncmp(line,"S3K_CD_ERROR:",13))snprintf(detail,256,"%s",line+13);
}

static void output_chunk(FILE *log,const char *bytes,size_t count,
                         char line[4096],size_t *used,char detail[256],
                         void (*progress)(void *,float,const char *),void *user)
{
    fwrite(bytes,1,count,log);fflush(log);
    for(size_t i=0;i<count;i++) {
        if(bytes[i]=='\n'||*used==4095) {
            line[*used]=0;output_line(line,detail,progress,user);*used=0;
        }
        if(bytes[i]!='\n')line[(*used)++]=bytes[i];
    }
}

static int media_import(unsigned kind,const char *disc,const char *rom,const char *settings,
                 char content[1024],char music[1024],char *error,size_t capacity,
                 void (*progress)(void *,float,const char *),void *user)
{
    char root[1024],cache[1200],receipt[1280],log_path[1280],script[1200],helper[1200],builder[1200];
    char exe_dir[1024]={0},line[4096],detail[256]={0};int code=1;
    content[0]=music[0]=0;parent(root,sizeof root,settings);
    if(kind<1 || kind>5 || !rom||!*rom||!disc||!*disc){snprintf(error,capacity,"Select your cartridge and owned media first.");return 0;}
    if(!join(cache,sizeof cache,root,kind==5?"sonic-sound-donors":kind==4?"sonic-cd-imports":"sonic-rom-imports"))goto path_error;
#ifdef _WIN32
    unsigned long pid=GetCurrentProcessId();
    if(_mkdir(cache)&&errno!=EEXIST)goto io_error;
    DWORD exe_length=GetModuleFileNameA(NULL,exe_dir,sizeof exe_dir);
    if(!exe_length||exe_length>=sizeof exe_dir)goto path_error;
    parent(exe_dir,sizeof exe_dir,exe_dir);
#else
    unsigned long pid=(unsigned long)getpid();
    if(mkdir(cache,0700)&&errno!=EEXIST)goto io_error;
    /* Packaged launchers run from their application directory. Source builds
     * also have the exact converter path compiled into this target. */
    snprintf(exe_dir,sizeof exe_dir,".");
#ifdef __linux__
    ssize_t length=readlink("/proc/self/exe",exe_dir,sizeof exe_dir-1);
    if(length>0&&(size_t)length<sizeof exe_dir-1){exe_dir[length]=0;parent(exe_dir,sizeof exe_dir,exe_dir);}
    else snprintf(exe_dir,sizeof exe_dir,".");
#endif
#endif
    if(snprintf(receipt,sizeof receipt,"%s/result-%lu.ini",cache,pid)>=(int)sizeof receipt ||
       snprintf(log_path,sizeof log_path,"%s/import-%lu.log",cache,pid)>=(int)sizeof log_path)goto path_error;
    const char *override=getenv("SONIC3K_IMPORT_HELPER");
    if(override&&*override) {
        if(strlen(override)>=sizeof builder)goto path_error;
        strcpy(builder,override);
        if(!exists(builder)){snprintf(error,capacity,"The selected import helper is unavailable.");return 0;}
    } else if(!join(builder,sizeof builder,exe_dir,
#ifdef _WIN32
                    "SonicSoundImporter.exe"
#else
                    "SonicSoundImporter"
#endif
    ))goto path_error;
    int bundled=exists(builder);
    const char *script_name=kind==4?"tools/import_sonic_cd.py":"tools/import_sound_donor.py";
    if(!join(script,sizeof script,exe_dir,script_name))goto path_error;
    if(!exists(script)) {
        if(!join(script,sizeof script,root,script_name))goto path_error;
        if(!exists(script))snprintf(script,sizeof script,"%s",kind==4?S3_CD_IMPORT_SOURCE:S3_ROM_IMPORT_SOURCE);
    }
    if(!bundled&&!exists(script)){snprintf(error,capacity,"Import tools are missing. Extract the complete game package.");return 0;}
    if(!join(helper,sizeof helper,exe_dir,
#ifdef _WIN32
             "scd_pack_validate.exe"
#else
             "scd_pack_validate"
#endif
    ))goto path_error;
    if(!exists(helper))snprintf(helper,sizeof helper,"%s",S3_CD_IMPORT_VALIDATOR);
    if(kind==4&&!exists(helper)){snprintf(error,capacity,"Sonic CD import validator is missing beside the game executable.");return 0;}
    if(remove(receipt)&&errno!=ENOENT)goto io_error;
    FILE *log=fopen(log_path,"wb");if(!log)goto io_error;
    if(progress)progress(user,-1,kind==5?"Verifying your sound donor and preparing its themes...":kind==4?"Checking your owned disc and preparing stages and music...":"Checking your owned cartridge and preparing its campaign...");
    const char *python=getenv("SONIC3K_PYTHON");
    const char *campaigns[]={"","sonic1","sonic2","megamix","sonic_cd","sound"};
    const char *args[20];unsigned argc=0;
    args[argc++]=bundled?builder:python&&*python?python:
#ifdef _WIN32
        "py.exe";
#else
        "python3";
#endif
    if(!bundled)args[argc++]=script;
    if(bundled){args[argc++]="--campaign";args[argc++]=campaigns[kind];}
    args[argc++]=!bundled&&kind==4?"--disc":"--source";args[argc++]=disc;
    args[argc++]="--expected";args[argc++]=rom;args[argc++]="--out";args[argc++]=cache;
    args[argc++]="--receipt";args[argc++]=receipt;
    if(kind==4){args[argc++]="--validator";args[argc++]=helper;}
    args[argc]=NULL;
#ifdef _WIN32
    wchar_t executable[32768],command[32768]={0},part[4096];size_t used=0;
    int python_launcher=!bundled&&!(python&&*python);
    HANDLE rd=NULL,wr=NULL;SECURITY_ATTRIBUTES sa={sizeof sa,NULL,TRUE};
    PROCESS_INFORMATION process={0};STARTUPINFOW start={sizeof start};
    int found=wide(args[0],part,4096)&&SearchPathW(NULL,part,NULL,32768,executable,NULL);
    if(!found&&python_launcher) {
        args[0]="python.exe";python_launcher=0;
        found=SearchPathW(NULL,L"python.exe",NULL,32768,executable,NULL)!=0;
    }
    /* WindowsApps' python.exe can open the Store. Prefer the installed Python
     * launcher and never activate that alias from a background import. */
    if(!found||wcsstr(executable,L"\\Microsoft\\WindowsApps\\python.exe")) {
        fclose(log);snprintf(error,capacity,"Import helper unavailable. Extract the complete game package.");return 0;
    }
    for(unsigned i=0;args[i];i++) {
        if(!wide(args[i],part,4096)||!argument(command,32768,&used,part)) {
            fclose(log);goto path_error;
        }
        if(i==0&&python_launcher) {
            /* py's selector is parsed before CRT argv unquoting: "-3" is
             * forwarded to Python, while the literal trusted -3 selects it. */
            if(used+4>=32768){fclose(log);goto path_error;}
            wmemcpy(command+used,L"-3 ",4);used+=3;
        }
    }
    if(!CreatePipe(&rd,&wr,&sa,0)||!SetHandleInformation(rd,HANDLE_FLAG_INHERIT,0)) {
        if(rd)CloseHandle(rd);if(wr)CloseHandle(wr);fclose(log);goto io_error;
    }
    start.dwFlags=STARTF_USESTDHANDLES;start.hStdOutput=start.hStdError=wr;
    start.hStdInput=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,NULL);
    int launched=CreateProcessW(executable,command,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&start,&process)!=0;
    CloseHandle(wr);if(start.hStdInput!=INVALID_HANDLE_VALUE)CloseHandle(start.hStdInput);
    if(!launched){CloseHandle(rd);fclose(log);snprintf(error,capacity,"Unable to start the owned-media importer.");return 0;}
    /* A console host or descendant can retain stdout after Python exits.
     * Completion belongs to this child, not to every inherited pipe handle.
     * Read only available bytes, then drain the final snapshot on child exit. */
    size_t line_used=0;char bytes[4096];
    for(;;) {
        int finished=WaitForSingleObject(process.hProcess,0)==WAIT_OBJECT_0;
        DWORD available=0;
        if(PeekNamedPipe(rd,NULL,0,NULL,&available,NULL)&&available) {
            do {
                DWORD got=0,count=available>sizeof bytes?sizeof bytes:available;
                if(!ReadFile(rd,bytes,count,&got,NULL)||!got)break;
                output_chunk(log,bytes,got,line,&line_used,detail,progress,user);
                available-=got;
            }while(available);
        }
        if(finished)break;
        WaitForSingleObject(process.hProcess,25);
    }
    CloseHandle(rd);
#else
    int pipefd[2];if(pipe(pipefd)){fclose(log);goto io_error;}
    pid_t child=fork();
    if(child==0) {
        close(pipefd[0]);dup2(pipefd[1],STDOUT_FILENO);dup2(pipefd[1],STDERR_FILENO);close(pipefd[1]);
        execvp(args[0],(char *const *)args);_exit(127);
    }
    close(pipefd[1]);
    if(child<0){close(pipefd[0]);fclose(log);goto io_error;}
    int flags=fcntl(pipefd[0],F_GETFL,0);
    if(flags<0||fcntl(pipefd[0],F_SETFL,flags|O_NONBLOCK)<0) {
        close(pipefd[0]);fclose(log);
        while(waitpid(child,NULL,0)<0&&errno==EINTR){}
        goto io_error;
    }
    int status=0;pid_t waited=0;
    size_t line_used=0;char bytes[4096];
    for(;;) {
        do{waited=waitpid(child,&status,WNOHANG);}while(waited<0&&errno==EINTR);
        /* Bound the final drain even if a descendant continues writing. */
        for(size_t drained=0;drained<65536;drained+=sizeof bytes) {
            ssize_t got=read(pipefd[0],bytes,sizeof bytes);
            if(got<0&&errno==EINTR)continue;
            if(got<=0)break;
            output_chunk(log,bytes,(size_t)got,line,&line_used,detail,progress,user);
        }
        if(waited!=0)break;
        struct pollfd pending={pipefd[0],POLLIN,0};poll(&pending,1,25);
    }
    close(pipefd[0]);
    code=waited==child&&WIFEXITED(status)?WEXITSTATUS(status):1;
#endif
    if(line_used){line[line_used]=0;output_line(line,detail,progress,user);}
    fclose(log);
#ifdef _WIN32
    WaitForSingleObject(process.hProcess,INFINITE);DWORD exit_code=1;
    GetExitCodeProcess(process.hProcess,&exit_code);code=(int)exit_code;
    CloseHandle(process.hThread);CloseHandle(process.hProcess);
#endif
    if(code){snprintf(error,capacity,"%s See %s",*detail?detail:"Owned-media import failed.",log_path);return 0;}
    FILE *result=fopen(receipt,"rb");if(!result)goto io_error;int format=0,audio_absent=0;
    while(fgets(line,sizeof line,result)) {
        line[strcspn(line,"\r\n")]=0;
        if(!strcmp(line,"format=1"))format=1;
        else if(kind==4&&!strcmp(line,"audio=absent"))audio_absent=1;
        else if(!strncmp(line,"content=",8)&&strlen(line+8)<1024)strcpy(content,line+8);
        else if(kind==4&&!strncmp(line,"music=",6)&&strlen(line+6)<1024)strcpy(music,line+6);
        else if(kind==3&&!strncmp(line,"rom=",4)&&strlen(line+4)<1024)strcpy(music,line+4);
    }
    fclose(result);remove(receipt);
    if(format&&*content&&((kind==1||kind==2||kind==5)||*music||(kind==4&&audio_absent)))return 1;
    snprintf(error,capacity,"Importer returned an incomplete installation; previous selection retained.");return 0;
path_error:snprintf(error,capacity,"Import path is too long or invalid.");return 0;
io_error:snprintf(error,capacity,"Unable to read or write the private import directory.");return 0;
}

int s2_music_import_file(const char *source,const char *expected,const char *settings,char content[1024],char *error,size_t capacity,void (*progress)(void *,float,const char *),void *user)
{char unused[1024];return media_import(5,source,expected,settings,content,unused,error,capacity,progress,user);}
