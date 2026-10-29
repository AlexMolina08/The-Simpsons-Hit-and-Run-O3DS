/* Bounded InstallShield extraction; libunshield verifies each saved file's MD5. */
#include <libunshield.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static Unshield* cabinet;
static char path[4096];

int cabinet_open(const char* source)
{
    if(cabinet) unshield_close(cabinet);
    unshield_set_log_level(UNSHIELD_LOG_LEVEL_ERROR);
    cabinet = unshield_open(source);
    return cabinet != NULL;
}
int cabinet_count(void) { return cabinet ? unshield_file_count(cabinet) : 0; }
double cabinet_size(int index) { return cabinet ? (double)unshield_file_size(cabinet, index) : 0; }
const char* cabinet_name(int index)
{
    if(!cabinet || index < 0 || index >= cabinet_count() || !unshield_file_is_valid(cabinet,index)) return "";
    const char* name = unshield_file_name(cabinet,index);
    if(!name) return "";
    const char* suffix = strrchr(name,'.');
    if(suffix && !strcasecmp(suffix,".rcf")) {
        if(snprintf(path,sizeof(path),"%s",name) >= (int)sizeof(path)) return "";
        return path;
    }
    const char* prefix = NULL;
    for(int i=0;i<unshield_file_group_count(cabinet);++i) {
        UnshieldFileGroup* group = unshield_file_group_get(cabinet,i);
        if(!group || index < (int)group->first_file || index > (int)group->last_file) continue;
        if(!strcasecmp(group->name,"Art")) prefix = "art/";
        else if(!strcasecmp(group->name,"Scripts")) prefix = "scripts/";
        else if(!strcasecmp(group->name,"Sound")) prefix = "";
        else if(!strcasecmp(group->name,"Movies")) prefix = "movies/";
        if(prefix) break;
    }
    if(!prefix) return "";
    int directory = unshield_file_directory(cabinet,index);
    const char* folder = directory >= 0 ? unshield_directory_name(cabinet,directory) : "";
    if(!folder) folder = "";
    int count = snprintf(path,sizeof(path),"%s%s%s%s",prefix,folder,*folder?"/":"",name);
    if(count < 0 || count >= (int)sizeof(path)) return "";
    for(char* p=path;*p;++p) if(*p=='\\') *p='/';
    return path;
}
int cabinet_save(int index,const char* destination)
{
    return cabinet && unshield_file_save(cabinet,index,destination);
}
void cabinet_close(void) { if(cabinet) unshield_close(cabinet);cabinet=NULL; }
