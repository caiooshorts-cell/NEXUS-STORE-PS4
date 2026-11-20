#include <SDL2/SDL.h>
#include <proto-include.h>
#include "qrcodegen.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <cstdint>
#include <orbis/libkernel.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

extern "C" int sceSysmoduleLoadModule(int moduleId);
extern "C" int sceSysmoduleLoadModuleInternal(int moduleId);

static const int SCREEN_W = 1920;
static const int SCREEN_H = 1080;
static const int PHONE_PORT = 8765;
static const char* JOB_FILE = "/data/nexus-store-downloads.txt";

static const SDL_Color BG       = { 5, 12, 24, 255 };
static const SDL_Color PANEL    = { 11, 24, 41, 255 };
static const SDL_Color PANEL2   = { 16, 34, 55, 255 };
static const SDL_Color PANEL3   = { 21, 46, 71, 255 };
static const SDL_Color CYAN     = { 49, 190, 232, 255 };
static const SDL_Color MINT     = { 55, 215, 174, 255 };
static const SDL_Color WHITE    = { 237, 245, 252, 255 };
static const SDL_Color MUTED    = { 133, 157, 180, 255 };
static const SDL_Color ORANGE   = { 255, 177, 91, 255 };
static const SDL_Color RED      = { 238, 112, 121, 255 };
static const SDL_Color BORDER   = { 33, 62, 88, 255 };

static const char* NAV_ITEMS[] = {
    "INICIO", "JOGOS", "APLICATIVOS", "DOWNLOADS", "CONECTAR"
};
static const int NAV_COUNT = 5;

struct InstallJob {
    std::string name;
    std::string titleId;
    std::string url;
    bool update;
    std::string state;
};

struct TextTexture {
    std::string text;
    int size;
    SDL_Color color;
    SDL_Texture* texture;
    int width;
    int height;
};

static SDL_Window* gWindow = NULL;
static SDL_Renderer* gRenderer = NULL;
static FT_Library gFtLibrary = NULL;
static FT_Face gFont = NULL;
static std::vector<TextTexture> gTextCache;

static std::mutex gJobsMutex;
static std::vector<InstallJob> gJobs;

static std::mutex gQueueMutex;
static std::vector<std::string> gIncomingUrls;

static std::atomic<bool> gStopServer(false);
static std::atomic<bool> gServerReady(false);
static std::thread gServerThread;
static int gServerSocket = -1;

static std::string gIpAddress = "IP NAO DISPONIVEL";
static std::string gPairKey;
static std::string gPairUrl;
static qrcodegen::QrCode* gQrCode = NULL;
static std::string gLastMessage = "PRONTO PARA CONECTAR";
static bool gOnline = false;
static std::mutex gBgftMutex;
static int gBgftModule = -1;
static int gUserServiceModule = -1;
static int gAppInstUtilModule = -1;
static bool gUserServiceInitialized = false;
static bool gAppInstUtilInitialized = false;
static bool gBgftInitialized = false;
static std::vector<unsigned char> gBgftHeap;

struct BgftInitParams {
    void* heap;
    size_t heapSize;
};
struct BgftDownloadParams {
    int32_t userId;
    int32_t entitlementType;
    const char* id;
    const char* contentUrl;
    const char* contentExUrl;
    const char* contentName;
    const char* iconPath;
    const char* skuId;
    int32_t option;
    const char* playgoScenarioId;
    const char* releaseDate;
    const char* packageType;
    const char* packageSubType;
    uint32_t packageSize;
};
static_assert(offsetof(BgftDownloadParams, packageSize) == 96, "BGFT parameter ABI mismatch");
static_assert(sizeof(BgftDownloadParams) == 104, "BGFT parameter ABI mismatch");

using BgftInitFn = int (*)(BgftInitParams*);
using BgftRegisterPkgFn = int (*)(BgftDownloadParams*, int*);
using BgftStartTaskFn = int (*)(int);
static BgftInitFn gBgftInit = NULL;
static BgftRegisterPkgFn gBgftRegisterPkg = NULL;
static BgftStartTaskFn gBgftStartTask = NULL;
static int gSelectedNav = 0;
static int gSelectedDownload = 0;
static SDL_Joystick* gController = NULL;

static void setColor(SDL_Renderer* r, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

static void fillRect(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c)
{
    SDL_Rect rect = { x, y, w, h };
    setColor(r, c);
    SDL_RenderFillRect(r, &rect);
}

static void fillRoundRect(SDL_Renderer* r, int x, int y, int w, int h, int radius, SDL_Color c)
{
    if (radius < 1) {
        fillRect(r, x, y, w, h, c);
        return;
    }
    if (radius > h / 2) radius = h / 2;
    if (radius > w / 2) radius = w / 2;
    setColor(r, c);
    for (int row = 0; row < h; ++row) {
        int dy = 0;
        if (row < radius) dy = radius - row;
        else if (row >= h - radius) dy = row - (h - radius - 1);
        int dx = 0;
        if (dy > 0) {
            double inside = (double)radius * radius - (double)(radius - dy) * (radius - dy);
            if (inside < 0.0) inside = 0.0;
            dx = radius - (int)std::sqrt(inside);
        }
        SDL_RenderDrawLine(r, x + dx, y + row, x + w - 1 - dx, y + row);
    }
}

static void outlineRoundRect(SDL_Renderer* r, int x, int y, int w, int h, int radius, SDL_Color c)
{
    if (radius < 1) {
        SDL_Rect rect = {x,y,w,h};
        setColor(r,c);
        SDL_RenderDrawRect(r,&rect);
        return;
    }
    setColor(r,c);
    SDL_RenderDrawLine(r,x+radius,y,x+w-radius-1,y);
    SDL_RenderDrawLine(r,x+radius,y+h-1,x+w-radius-1,y+h-1);
    SDL_RenderDrawLine(r,x,y+radius,x,y+h-radius-1);
    SDL_RenderDrawLine(r,x+w-1,y+radius,x+w-1,y+h-radius-1);
    for (int i=0;i<=radius;i++) {
        int d = radius - (int)std::sqrt((double)radius*radius-(double)i*i);
        SDL_RenderDrawPoint(r,x+radius-d,y+radius-i);
        SDL_RenderDrawPoint(r,x+w-radius-1+d,y+radius-i);
        SDL_RenderDrawPoint(r,x+radius-d,y+h-radius-1+i);
        SDL_RenderDrawPoint(r,x+w-radius-1+d,y+h-radius-1+i);
    }
}

static bool sameColor(SDL_Color a, SDL_Color b)
{
    return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a;
}

static TextTexture* getTextTexture(const std::string& text, int pixelSize, SDL_Color color)
{
    for (size_t i=0;i<gTextCache.size();++i) {
        TextTexture& t = gTextCache[i];
        if (t.text == text && t.size == pixelSize && sameColor(t.color,color))
            return &t;
    }
    if (!gFont || text.empty()) return NULL;

    FT_Set_Pixel_Sizes(gFont, 0, pixelSize);
    int width = 4;
    int height = pixelSize + pixelSize/2 + 6;
    for (size_t i=0;i<text.size();++i) {
        if (FT_Load_Char(gFont, (unsigned char)text[i], FT_LOAD_DEFAULT)==0)
            width += (int)(gFont->glyph->advance.x >> 6) + 1;
    }
    if (width < 2) width = 2;

    SDL_Surface* surface = SDL_CreateRGBSurface(0,width,height,32,
        0x00FF0000,0x0000FF00,0x000000FF,0xFF000000);
    if (!surface) return NULL;
    SDL_FillRect(surface,NULL,SDL_MapRGBA(surface->format,0,0,0,0));
    SDL_LockSurface(surface);

    int penX = 2;
    int baseline = pixelSize + 2;
    for (size_t i=0;i<text.size();++i) {
        unsigned char ch = (unsigned char)text[i];
        if (FT_Load_Char(gFont,ch,FT_LOAD_RENDER)!=0) continue;
        FT_GlyphSlot slot = gFont->glyph;
        FT_Bitmap& bm = slot->bitmap;
        int top = baseline - slot->bitmap_top;
        int left = penX + slot->bitmap_left;
        for (unsigned int by=0; by<bm.rows; ++by) {
            int srcRow = bm.pitch >= 0 ? (int)by*bm.pitch : (int)(bm.rows-1-by)*(-bm.pitch);
            const unsigned char* src = bm.buffer + srcRow;
            for (unsigned int bx=0; bx<bm.width; ++bx) {
                unsigned char alpha = src[bx];
                if (bm.pixel_mode == FT_PIXEL_MODE_MONO)
                    alpha = (src[bx/8] & (0x80 >> (bx%8))) ? 255 : 0;
                if (!alpha) continue;
                int px = left + (int)bx;
                int py = top + (int)by;
                if (px<0 || py<0 || px>=width || py>=height) continue;
                Uint32* row = (Uint32*)((Uint8*)surface->pixels + py*surface->pitch);
                row[px] = SDL_MapRGBA(surface->format,color.r,color.g,color.b,alpha);
            }
        }
        penX += (int)(slot->advance.x >> 6);
    }

    SDL_UnlockSurface(surface);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(gRenderer,surface);
    int textW = surface->w;
    int textH = surface->h;
    SDL_FreeSurface(surface);
    if (!texture) return NULL;
    SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);

    TextTexture entry;
    entry.text=text;
    entry.size=pixelSize;
    entry.color=color;
    entry.texture=texture;
    entry.width=textW;
    entry.height=textH;
    gTextCache.push_back(entry);
    return &gTextCache.back();
}

static void drawText(SDL_Renderer* r, int x, int y, const std::string& text, int size, SDL_Color color)
{
    TextTexture* t = getTextTexture(text,size,color);
    if (!t || !t->texture) return;
    SDL_Rect dst = {x,y,t->width,t->height};
    SDL_RenderCopy(r,t->texture,NULL,&dst);
}

static void drawTextCentered(SDL_Renderer* r, int cx, int y, const std::string& text, int size, SDL_Color color)
{
    TextTexture* t = getTextTexture(text,size,color);
    if (!t || !t->texture) return;
    SDL_Rect dst = {cx-t->width/2,y,t->width,t->height};
    SDL_RenderCopy(r,t->texture,NULL,&dst);
}

static std::string toUpper(std::string s)
{
    for(size_t i=0;i<s.size();++i) s[i]=(char)std::toupper((unsigned char)s[i]);
    return s;
}

static std::string urlEncode(const std::string& s)
{
    static const char* hex="0123456789ABCDEF";
    std::string out;
    for(size_t i=0;i<s.size();++i) {
        unsigned char c=(unsigned char)s[i];
        if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')
            out.push_back((char)c);
        else {
            out.push_back('%'); out.push_back(hex[c>>4]); out.push_back(hex[c&15]);
        }
    }
    return out;
}

static std::string urlDecode(const std::string& s)
{
    std::string out;
    for(size_t i=0;i<s.size();++i) {
        if(s[i]=='+') out.push_back(' ');
        else if(s[i]=='%' && i+2<s.size()) {
            char hex[3]={s[i+1],s[i+2],0};
            out.push_back((char)strtol(hex,NULL,16));
            i+=2;
        } else out.push_back(s[i]);
    }
    return out;
}

static std::string queryValue(const std::string& query, const std::string& key)
{
    size_t start=0;
    while(start<query.size()) {
        size_t end=query.find('&',start);
        if(end==std::string::npos) end=query.size();
        std::string part=query.substr(start,end-start);
        size_t eq=part.find('=');
        if(eq!=std::string::npos && part.substr(0,eq)==key)
            return urlDecode(part.substr(eq+1));
        start=end+1;
    }
    return "";
}

static std::string formValue(const std::string& body, const std::string& key)
{
    return queryValue(body,key);
}

static std::string jsonEscape(const std::string& s)
{
    std::string out;
    for(size_t i=0;i<s.size();++i) {
        char c=s[i];
        if(c=='"' || c=='\\') { out.push_back('\\'); out.push_back(c); }
        else if(c=='\n') out += "\\n";
        else if(c=='\r') out += "\\r";
        else if((unsigned char)c>=32) out.push_back(c);
    }
    return out;
}

static std::string urlFileName(const std::string& url)
{
    std::string path=url;
    size_t q=path.find('?'); if(q!=std::string::npos) path=path.substr(0,q);
    size_t hash=path.find('#'); if(hash!=std::string::npos) path=path.substr(0,hash);
    size_t slash=path.find_last_of('/');
    std::string name=slash==std::string::npos?path:path.substr(slash+1);
    if(name.empty()) name="PACKAGE.pkg";
    name=urlDecode(name);
    return name;
}

static std::string titleIdFromText(const std::string& raw)
{
    std::string s=toUpper(raw);
    const char* prefixes[]={"CUSA","PPSA","PCAS","PCJS"};
    for(size_t p=0;p<4;++p) {
        size_t at=s.find(prefixes[p]);
        if(at!=std::string::npos && at+9<=s.size()) {
            bool digits=true;
            for(size_t i=at+4;i<at+9;++i)
                if(!std::isdigit((unsigned char)s[i])) digits=false;
            if(digits) return s.substr(at,9);
        }
    }
    return "";
}

static std::string friendlyName(const std::string& url)
{
    std::string name=urlFileName(url);
    size_t ext=name.find_last_of('.');
    if(ext!=std::string::npos) name=name.substr(0,ext);
    for(size_t i=0;i<name.size();++i) {
        if(name[i]=='_' || name[i]=='-') name[i]=' ';
    }
    while(!name.empty() && name[0]==' ') name.erase(name.begin());
    return name.empty()?"PACOTE PS4":name;
}

static bool looksLikeUpdate(const std::string& url)
{
    std::string s=toUpper(url);
    return s.find("UPDATE")!=std::string::npos ||
           s.find("PATCH")!=std::string::npos ||
           s.find("CUMULATIVE")!=std::string::npos ||
           s.find("DELTA")!=std::string::npos ||
           s.find("UP000")!=std::string::npos ||
           s.find("/UP")!=std::string::npos;
}

static bool sendAll(int fd, const char* data, size_t length)
{
    size_t sent=0;
    while(sent<length) {
        int n=(int)send(fd,data+sent,length-sent,0);
        if(n<=0) return false;
        sent+=(size_t)n;
    }
    return true;
}

static std::string bgftErrorCode(int rc)
{
    char code[16];
    snprintf(code,sizeof(code),"0x%08X",(unsigned int)rc);
    return code;
}

// Internal PS4 modules must be requested by their sysmodule ID. Loading
// "/system/common/lib/libSceBgft.sprx" as an ordinary file returns ENOENT
// on retail firmware. Once loaded, find the module handle and resolve exports.
static int findLoadedNativeModule(const char* name)
{
    OrbisKernelModule handles[256] = {};
    size_t count = 0;
    int rc = sceKernelGetModuleList(handles,sizeof(handles),&count);
    if(rc!=0) return -1;
    if(count>256) count=256;

    for(size_t i=0;i<count;++i) {
        OrbisKernelModuleInfo info;
        memset(&info,0,sizeof(info));
        info.size=sizeof(info);
        if(sceKernelGetModuleInfo(handles[i],&info)!=0) continue;
        if(strstr(info.name,name)!=NULL) return (int)handles[i];
    }
    return -1;
}

static bool loadInternalModule(int moduleId,const char* name,int& module,std::string& error)
{
    module=findLoadedNativeModule(name);
    if(module>=0) return true;

    int rc=sceSysmoduleLoadModuleInternal(moduleId);
    if(rc!=0) {
        // Check once more in case another system component loaded it while
        // the sysmodule request was being processed.
        module=findLoadedNativeModule(name);
        if(module>=0) return true;
        error=std::string("NAO FOI POSSIVEL CARREGAR ")+name+" PELO SYSMODULE: "+bgftErrorCode(rc);
        return false;
    }

    module=findLoadedNativeModule(name);
    if(module<0) {
        error=std::string(name)+" FOI SOLICITADO, MAS NAO APARECEU NA LISTA DE MODULOS DO PS4.";
        return false;
    }
    return true;
}

template<typename T>
static T resolveBgftSymbol(int module,const char* name)
{
    T function=NULL;
    int rc=sceKernelDlsym(module,name,reinterpret_cast<void**>(&function));
    if(rc!=0 || !function) return NULL;
    return function;
}

using UserServiceInitFn = int (*)(void*);
using GetForegroundUserFn = int (*)(int*);
using AppInstUtilInitFn = int (*)();

static bool initializeUserService(std::string& error)
{
    if(gUserServiceInitialized) return true;
    if(!loadInternalModule(0x80000011,"libSceUserService",gUserServiceModule,error)) return false;

    UserServiceInitFn initialize=resolveBgftSymbol<UserServiceInitFn>(gUserServiceModule,"sceUserServiceInitialize");
    if(!initialize) {
        error="NAO FOI POSSIVEL RESOLVER sceUserServiceInitialize.";
        return false;
    }
    int rc=initialize(NULL);
    if(rc!=0) {
        error="FALHA AO INICIAR O SERVICO DE USUARIO: "+bgftErrorCode(rc);
        return false;
    }
    gUserServiceInitialized=true;
    return true;
}

static bool initializeAppInstUtil(std::string& error)
{
    if(gAppInstUtilInitialized) return true;
    if(!loadInternalModule(0x80000014,"libSceAppInstUtil",gAppInstUtilModule,error)) return false;

    AppInstUtilInitFn initialize=resolveBgftSymbol<AppInstUtilInitFn>(gAppInstUtilModule,"sceAppInstUtilInitialize");
    if(!initialize) {
        error="NAO FOI POSSIVEL RESOLVER sceAppInstUtilInitialize.";
        return false;
    }
    int rc=initialize();
    if(rc!=0) {
        error="FALHA AO INICIAR O SERVICO DE INSTALACAO: "+bgftErrorCode(rc);
        return false;
    }
    gAppInstUtilInitialized=true;
    return true;
}

static bool initializeBgft(std::string& error)
{
    std::lock_guard<std::mutex> lock(gBgftMutex);
    if(gBgftInitialized) return true;
    if(!loadInternalModule(0x8000002A,"libSceBgft",gBgftModule,error)) return false;

    // OpenOrbis names this export sceBgftServiceIntInit (not sceBgftServiceInit).
    gBgftInit=resolveBgftSymbol<BgftInitFn>(gBgftModule,"sceBgftServiceIntInit");
    gBgftRegisterPkg=resolveBgftSymbol<BgftRegisterPkgFn>(gBgftModule,"sceBgftServiceIntDebugDownloadRegisterPkg");
    gBgftStartTask=resolveBgftSymbol<BgftStartTaskFn>(gBgftModule,"sceBgftServiceDownloadStartTask");
    if(!gBgftInit || !gBgftRegisterPkg || !gBgftStartTask) {
        error="O MODULO BGFT FOI CARREGADO, MAS NAO FOI POSSIVEL RESOLVER TODAS AS FUNCOES.";
        return false;
    }

    gBgftHeap.assign(1024*1024,0);
    BgftInitParams initParams;
    initParams.heap=gBgftHeap.data();
    initParams.heapSize=gBgftHeap.size();
    int rc=gBgftInit(&initParams);
    if(rc!=0) {
        error="FALHA AO INICIAR O BGFT: "+bgftErrorCode(rc);
        return false;
    }
    gBgftInitialized=true;
    return true;
}

// Registra a URL diretamente no serviço nativo de downloads/instalação do PS4.
// Não usa Remote Package Installer nem a porta TCP 12800.
static bool submitInstall(const std::string& url,const std::string& packageName,std::string& result)
{
    if(!initializeUserService(result) || !initializeAppInstUtil(result) || !initializeBgft(result))
        return false;

    GetForegroundUserFn getForegroundUser=resolveBgftSymbol<GetForegroundUserFn>(gUserServiceModule,"sceUserServiceGetForegroundUser");
    int userId=-1;
    if(!getForegroundUser || getForegroundUser(&userId)!=0 || userId<0) {
        result="ENTRE EM UM PERFIL DO PS4 ANTES DE INSTALAR.";
        return false;
    }

    BgftDownloadParams params;
    memset(&params,0,sizeof(params));
    params.userId=userId;
    params.entitlementType=5;
    params.id="";
    params.contentUrl=url.c_str();
    params.contentExUrl="";
    params.contentName=packageName.c_str();
    params.iconPath="";
    params.skuId="";
    params.option=0x10000;
    params.playgoScenarioId="0";
    params.releaseDate="";
    params.packageType="";
    params.packageSubType="";
    params.packageSize=0;

    int taskId=-1;
    int rc=gBgftRegisterPkg(&params,&taskId);
    if(rc!=0) {
        result="O PS4 RECUSOU O REGISTRO DO PKG: "+bgftErrorCode(rc);
        return false;
    }
    if(taskId<0) {
        result="O BGFT NAO RETORNOU UM ID DE DOWNLOAD VALIDO.";
        return false;
    }

    rc=gBgftStartTask(taskId);
    if(rc!=0) {
        result="NAO FOI POSSIVEL INICIAR O DOWNLOAD NATIVO: "+bgftErrorCode(rc);
        return false;
    }

    result="TAREFA NATIVA BGFT #"+std::to_string(taskId);
    return true;
}

static void saveJob(const InstallJob& job)
{
    FILE* f=fopen(JOB_FILE,"ab");
    if(!f) return;
    fprintf(f,"%s|%d|%s|%s\n",job.titleId.c_str(),job.update?1:0,
            job.name.c_str(),job.url.c_str());
    fclose(f);
}

static void loadJobs()
{
    FILE* f=fopen(JOB_FILE,"rb");
    if(!f) return;
    char line[4096];
    while(fgets(line,sizeof(line),f)) {
        std::string s=line;
        while(!s.empty() && (s[s.size()-1]=='\n'||s[s.size()-1]=='\r')) s.erase(s.size()-1);
        size_t a=s.find('|'); if(a==std::string::npos) continue;
        size_t b=s.find('|',a+1); if(b==std::string::npos) continue;
        size_t c=s.find('|',b+1); if(c==std::string::npos) continue;
        InstallJob job;
        job.titleId=s.substr(0,a);
        job.update=s.substr(a+1,b-a-1)=="1";
        job.name=s.substr(b+1,c-b-1);
        job.url=s.substr(c+1);
        job.state="DOWNLOAD NATIVO INICIADO";
        gJobs.push_back(job);
    }
    fclose(f);
}

static bool enqueueUrl(const std::string& url, std::string& message)
{
    if(url.size()<12 || url.size()>2048 ||
       !(url.compare(0,7,"http://")==0 || url.compare(0,8,"https://")==0)) {
        message="COLE UM LINK HTTP OU HTTPS VALIDO";
        return false;
    }
    std::string path=url.substr(0,url.find('?'));
    if(toUpper(path).find(".PKG")==std::string::npos) {
        message="O LINK PRECISA APONTAR PARA UM ARQUIVO PKG";
        return false;
    }

    std::string installerResponse;
    std::string packageName=friendlyName(url);
    if(!submitInstall(url,packageName,installerResponse)) {
        message=installerResponse.empty()?"FALHA AO INICIAR A INSTALACAO NATIVA. VERIFIQUE SE O GOLDHEN ESTA ATIVO.":installerResponse;
        return false;
    }

    InstallJob job;
    job.name=packageName;
    job.url=url;
    job.titleId=titleIdFromText(url);
    job.update=looksLikeUpdate(url);
    job.state="DOWNLOAD NATIVO INICIADO";
    {
        std::lock_guard<std::mutex> lock(gJobsMutex);
        gJobs.push_back(job);
    }
    saveJob(job);
    message="DOWNLOAD INICIADO PELO INSTALADOR NATIVO DO PS4";
    return true;
}

static std::string getHeaderValue(const std::string& headers,const std::string& key)
{
    std::string lower=headers;
    for(size_t i=0;i<lower.size();++i) lower[i]=(char)std::tolower((unsigned char)lower[i]);
    std::string find=key;
    for(size_t i=0;i<find.size();++i) find[i]=(char)std::tolower((unsigned char)find[i]);
    size_t at=lower.find(find+":");
    if(at==std::string::npos) return "";
    at=headers.find(':',at);
    if(at==std::string::npos) return "";
    ++at;
    while(at<headers.size() && (headers[at]==' '||headers[at]=='\t')) ++at;
    size_t end=headers.find("\r\n",at);
    if(end==std::string::npos) end=headers.size();
    return headers.substr(at,end-at);
}

static void replyHttp(int fd, int status, const char* phrase, const char* type,
                      const std::string& body, bool cors)
{
    std::ostringstream h;
    h<<"HTTP/1.1 "<<status<<" "<<phrase<<"\r\n"
     <<"Content-Type: "<<type<<"\r\n"
     <<"Content-Length: "<<body.size()<<"\r\n"
     <<"Connection: close\r\n";
    if(cors) {
        h<<"Access-Control-Allow-Origin: *\r\n"
         <<"Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
         <<"Access-Control-Allow-Headers: Content-Type\r\n"
         <<"Cache-Control: no-store\r\n";
    }
    h<<"\r\n";
    std::string header=h.str();
    sendAll(fd,header.c_str(),header.size());
    if(!body.empty()) sendAll(fd,body.c_str(),body.size());
}

static void handlePhoneRequest(int client)
{
    struct timeval timeout;
    timeout.tv_sec=5; timeout.tv_usec=0;
    setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));

    char buffer[16384];
    std::string request;
    size_t headerEnd=std::string::npos;
    size_t expectedBody=0;
    while(request.size()<sizeof(buffer)-1) {
        int n=(int)recv(client,buffer,sizeof(buffer)-1-request.size(),0);
        if(n<=0) break;
        request.append(buffer,n);
        headerEnd=request.find("\r\n\r\n");
        if(headerEnd!=std::string::npos) {
            std::string headers=request.substr(0,headerEnd);
            std::string len=getHeaderValue(headers,"content-length");
            if(!len.empty()) expectedBody=(size_t)atoi(len.c_str());
            if(request.size()>=headerEnd+4+expectedBody) break;
        }
    }
    if(request.empty()) return;
    headerEnd=request.find("\r\n\r\n");
    if(headerEnd==std::string::npos) {
        replyHttp(client,400,"Bad Request","application/json","{\"ok\":false}",true);
        return;
    }
    std::string headers=request.substr(0,headerEnd);
    std::string firstLine=request.substr(0,request.find("\r\n"));
    std::istringstream line(firstLine);
    std::string method,target,version;
    line>>method>>target>>version;
    size_t q=target.find('?');
    std::string path=q==std::string::npos?target:target.substr(0,q);
    std::string query=q==std::string::npos?"":target.substr(q+1);

    if(method=="OPTIONS") {
        replyHttp(client,204,"No Content","text/plain","",true);
        return;
    }

    std::string key=queryValue(query,"key");
    if(key!=gPairKey || key.empty()) {
        replyHttp(client,403,"Forbidden","application/json","{\"ok\":false,\"message\":\"QR DE PAREAMENTO INVALIDO OU EXPIRADO\"}",true);
        return;
    }

    if(method=="GET" && (path=="/" || path=="/connect.html")) {
        FILE* f=fopen("/app0/connect.html","rb");
        if(!f) {
            replyHttp(client,404,"Not Found","text/plain","Abra o site pelo QR code da NEXUS STORE.",true);
            return;
        }
        fseek(f,0,SEEK_END);
        long length=ftell(f);
        fseek(f,0,SEEK_SET);
        std::string body;
        if(length>0 && length<1000000) {
            body.resize((size_t)length);
            fread(&body[0],1,(size_t)length,f);
        }
        fclose(f);
        replyHttp(client,200,"OK","text/html; charset=utf-8",body,true);
        return;
    }

    if(method=="POST" && path=="/api/queue") {
        std::string body=request.substr(headerEnd+4);
        std::string url=formValue(body,"url");
        std::string message;
        bool ok=enqueueUrl(url,message);
        std::string json="{\"ok\":"+(ok?std::string("true"):std::string("false"))+
            ",\"message\":\""+jsonEscape(message)+"\"}";
        replyHttp(client,ok?200:503,ok?"OK":"Service Unavailable","application/json",json,true);
        return;
    }

    if(method=="GET" && path=="/api/status") {
        std::ostringstream json;
        json<<"{\"ok\":true,\"ip\":\""<<gIpAddress<<"\",\"server\":\""<<PHONE_PORT<<"\"}";
        replyHttp(client,200,"OK","application/json",json.str(),true);
        return;
    }

    replyHttp(client,404,"Not Found","application/json","{\"ok\":false,\"message\":\"ENDPOINT NAO ENCONTRADO\"}",true);
}

static void phoneServerLoop()
{
    int server=socket(AF_INET,SOCK_STREAM,0);
    if(server<0) {
        gLastMessage="FALHA AO ABRIR SERVIDOR DO CELULAR";
        return;
    }
    gServerSocket=server;
    int yes=1;
    setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(PHONE_PORT);

    if(bind(server,(struct sockaddr*)&addr,sizeof(addr))<0 || listen(server,6)<0) {
        gLastMessage="NAO FOI POSSIVEL ABRIR A PORTA 8765";
        close(server);
        gServerSocket=-1;
        return;
    }
    gServerReady=true;

    while(!gStopServer.load()) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(server,&readSet);
        struct timeval tv;
        tv.tv_sec=0; tv.tv_usec=250000;
        int ready=select(server+1,&readSet,NULL,NULL,&tv);
        if(ready<=0) continue;
        struct sockaddr_in remote;
        socklen_t remoteLen=sizeof(remote);
        int client=accept(server,(struct sockaddr*)&remote,&remoteLen);
        if(client>=0) {
            handlePhoneRequest(client);
            close(client);
        }
    }
    close(server);
    gServerSocket=-1;
    gServerReady=false;
}

static void createPairingQr();

static void refreshNetworkAddress(bool force=false)
{
    static Uint32 lastCheck=0;
    static bool moduleReady=false;
    Uint32 now=SDL_GetTicks();
    if(!force && (Uint32)(now-lastCheck)<3000) return;
    lastCheck=now;

    if(!moduleReady) {
        int netResult=sceSysmoduleLoadModuleInternal(0x8000001C);
        if(netResult<0) {
            gOnline=false;
            return;
        }
        moduleReady=true;
    }

    std::string previousIp=gIpAddress;
    int fd=socket(AF_INET,SOCK_DGRAM,0);
    if(fd>=0) {
        struct sockaddr_in remote;
        memset(&remote,0,sizeof(remote));
        remote.sin_family=AF_INET;
        remote.sin_port=htons(53);
        inet_pton(AF_INET,"8.8.8.8",&remote.sin_addr);
        if(connect(fd,(struct sockaddr*)&remote,sizeof(remote))==0) {
            struct sockaddr_in local;
            socklen_t localLen=sizeof(local);
            memset(&local,0,sizeof(local));
            if(getsockname(fd,(struct sockaddr*)&local,&localLen)==0) {
                char address[INET_ADDRSTRLEN];
                if(inet_ntop(AF_INET,&local.sin_addr,address,sizeof(address)) &&
                   strcmp(address,"0.0.0.0")!=0)
                    gIpAddress=address;
            }
        }
        close(fd);
    }

    gOnline=(gIpAddress!="IP NAO DISPONIVEL");
    if(gIpAddress!=previousIp && !gPairKey.empty())
        createPairingQr();
}

static void createPairingQr()
{
    if(gQrCode) {
        delete gQrCode;
        gQrCode=NULL;
    }
    std::srand((unsigned int)(std::time(NULL) ^ SDL_GetTicks() ^ (uintptr_t)&gPairKey));
    char token[24];
    snprintf(token,sizeof(token),"%08X%08X",(unsigned int)std::rand(),(unsigned int)std::rand());
    gPairKey=token;
    std::string server="http://"+gIpAddress+":"+std::to_string(PHONE_PORT);
    gPairUrl=server+"/?key="+gPairKey;
    try {
        gQrCode=new qrcodegen::QrCode(qrcodegen::QrCode::encodeText(gPairUrl.c_str(),qrcodegen::QrCode::Ecc::MEDIUM));
    } catch(...) {
        gQrCode=NULL;
    }
}

static void drawQrCode(SDL_Renderer* r,int x,int y,int boxSize)
{
    if(!gQrCode) return;
    int n=gQrCode->getSize();
    int total=n+8;
    int scale=boxSize/total;
    if(scale<2) scale=2;
    int actual=total*scale;
    int ox=x+(boxSize-actual)/2;
    int oy=y+(boxSize-actual)/2;
    fillRoundRect(r,ox-12,oy-12,actual+24,actual+24,18,{248,251,255,255});
    fillRect(r,ox,oy,actual,actual,{248,251,255,255});
    for(int yy=0;yy<n;++yy) {
        for(int xx=0;xx<n;++xx) {
            if(gQrCode->getModule(xx,yy)) {
                SDL_Rect mod={ox+(xx+4)*scale,oy+(yy+4)*scale,scale,scale};
                setColor(r,{5,17,29,255});
                SDL_RenderFillRect(r,&mod);
            }
        }
    }
}

static void addRecordFromUrl(const std::string& url)
{
    InstallJob job;
    job.name=friendlyName(url);
    job.url=url;
    job.titleId=titleIdFromText(url);
    job.update=looksLikeUpdate(url);
    job.state="ENVIADO AO INSTALADOR";
    std::lock_guard<std::mutex> lock(gJobsMutex);
    gJobs.push_back(job);
    saveJob(job);
}

static void drawHeader(SDL_Renderer* r)
{
    fillRoundRect(r,22,18,1876,86,24,PANEL);
    fillRoundRect(r,43,35,50,50,14,CYAN);
    drawTextCentered(r,68,47,"N",28,BG);
    drawText(r,112,35,"NEXUS STORE",30,WHITE);
    drawText(r,114,74,"PS4 HOME BREW",12,CYAN);
    fillRoundRect(r,1610,39,250,42,18,gOnline?SDL_Color{13,56,54,255}:SDL_Color{69,46,25,255});
    drawTextCentered(r,1735,52,gOnline?"REDE CONECTADA":"SEM CONEXAO",13,gOnline?MINT:ORANGE);
}

static void drawSidebar(SDL_Renderer* r)
{
    fillRoundRect(r,22,124,278,932,24,PANEL);
    drawText(r,48,154,"MENU",13,MUTED);
    for(int i=0;i<NAV_COUNT;++i) {
        int y=206+i*91;
        bool active=i==gSelectedNav;
        if(active) {
            fillRoundRect(r,38,y-14,246,66,18,PANEL3);
            fillRoundRect(r,38,y-14,5,66,3,CYAN);
        }
        drawText(r,62,y,i==0?"CASA":NAV_ITEMS[i],18,active?WHITE:MUTED);
        if(active) fillRoundRect(r,248,y+7,10,10,5,MINT);
    }
    fillRoundRect(r,42,740,238,2,2,BORDER);
    drawText(r,48,773,"CONTROLE",12,MUTED);
    drawText(r,48,808,"D-PAD  NAVEGAR",13,WHITE);
    drawText(r,48,842,"X  ABRIR",13,WHITE);
    drawText(r,48,876,"O  VOLTAR",13,WHITE);
    drawText(r,48,964,"NEXUS",23,CYAN);
    drawText(r,48,996,"STORE",23,WHITE);
}

static void drawHome(SDL_Renderer* r)
{
    fillRoundRect(r,330,126,1568,920,26,BG);
    fillRoundRect(r,360,154,1507,306,26,PANEL);
    fillRoundRect(r,360,154,7,306,4,CYAN);
    drawText(r,408,193,"BEM-VINDO A",15,MINT);
    drawText(r,408,244,"NEXUS STORE",54,WHITE);
    drawText(r,412,327,"UMA NOVA FORMA DE ORGANIZAR SEUS PACOTES NO PS4",19,CYAN);
    drawText(r,412,375,"CONECTE O CELULAR, ENVIE O LINK E ACOMPANHE AS INSTALACOES.",15,MUTED);
    fillRoundRect(r,1440,198,370,220,22,PANEL3);
    drawTextCentered(r,1625,238,"NEXUS",34,CYAN);
    drawTextCentered(r,1625,286,"STORE",34,WHITE);
    drawTextCentered(r,1625,358,gOnline?"PRONTO PARA CONECTAR":"CONECTE O PS4 A REDE",13,MUTED);

    drawText(r,365,503,"ACESSO RAPIDO",18,WHITE);
    const char* labels[]={"JOGOS","DOWNLOADS","CONECTAR"};
    const char* desc[]={"BIBLIOTECA E ATUALIZACOES","ACOMPANHE OS PACOTES","ESCANEIE O QR COM O CELULAR"};
    const char* sym[]={"01","02","03"};
    for(int i=0;i<3;++i) {
        int x=360+i*505;
        fillRoundRect(r,x,550,475,230,22,PANEL2);
        outlineRoundRect(r,x,550,475,230,22,i==2?CYAN:BORDER);
        fillRoundRect(r,x+27,580,62,62,17,i==2?CYAN:PANEL3);
        drawText(r,x+44,597,sym[i],16,i==2?BG:MINT);
        drawText(r,x+28,672,labels[i],23,WHITE);
        drawText(r,x+28,723,desc[i],12,MUTED);
    }
    fillRoundRect(r,360,818,1507,174,22,PANEL);
    drawText(r,392,850,"STATUS DO SISTEMA",13,MINT);
    drawText(r,392,891,gLastMessage,19,WHITE);
    drawText(r,392,941,gIpAddress,14,MUTED);
}

static void drawGenericPage(SDL_Renderer* r,int tab)
{
    fillRoundRect(r,330,126,1568,920,26,BG);
    drawText(r,370,164,NAV_ITEMS[tab],28,WHITE);
    if(tab==1) {
        drawText(r,370,228,"JOGOS BASE E ATUALIZACOES SERAO AGRUPADOS PELO TITLE ID.",16,MUTED);
    } else if(tab==2) {
        drawText(r,370,228,"APLICATIVOS E FERRAMENTAS ENVIADOS PARA O CONSOLE.",16,MUTED);
    }
    std::vector<InstallJob> jobs;
    {
        std::lock_guard<std::mutex> lock(gJobsMutex);
        jobs=gJobs;
    }
    int row=0;
    std::vector<std::string> seen;
    for(size_t i=0;i<jobs.size() && row<6;++i) {
        bool show=(tab==1)?!jobs[i].update:(tab==2?jobs[i].update:false);
        if(!show) continue;
        std::string key=jobs[i].titleId.empty()?jobs[i].name:jobs[i].titleId;
        if(std::find(seen.begin(),seen.end(),key)!=seen.end()) continue;
        seen.push_back(key);
        int y=280+row*112;
        fillRoundRect(r,370,y,1485,91,17,PANEL2);
        drawText(r,400,y+17,jobs[i].name,18,WHITE);
        drawText(r,400,y+54,jobs[i].titleId.empty()?"ID NAO IDENTIFICADO":jobs[i].titleId,12,MUTED);
        drawText(r,1510,y+34,jobs[i].state,12,MINT);
        ++row;
    }
    if(row==0) {
        fillRoundRect(r,370,285,1480,150,20,PANEL);
        drawText(r,406,326,tab==1?"NENHUM JOGO ENVIADO AINDA":"NENHUM ITEM NESTA AREA",20,WHITE);
        drawText(r,406,376,"USE A ABA CONECTAR PARA ENVIAR UM LINK PELO CELULAR.",14,MUTED);
    }
}

static void drawDownloads(SDL_Renderer* r)
{
    fillRoundRect(r,330,126,1568,920,26,BG);
    drawText(r,370,164,"DOWNLOADS E ATUALIZACOES",28,WHITE);
    drawText(r,370,215,"UPDATES COM O MESMO TITLE ID FICAM NO GRUPO DO JOGO.",14,MUTED);

    std::vector<InstallJob> jobs;
    {
        std::lock_guard<std::mutex> lock(gJobsMutex);
        jobs=gJobs;
    }
    if(jobs.empty()) {
        fillRoundRect(r,370,275,1480,170,20,PANEL);
        drawText(r,410,319,"NENHUMA INSTALACAO ENVIADA",21,WHITE);
        drawText(r,410,370,"ESCANEIE O QR CODE E COLE UM LINK PKG NO CELULAR.",14,MUTED);
        return;
    }

    std::vector<std::string> groups;
    int row=0;
    for(size_t i=0;i<jobs.size() && row<5;++i) {
        std::string group=jobs[i].titleId.empty()?jobs[i].name:jobs[i].titleId;
        if(std::find(groups.begin(),groups.end(),group)!=groups.end()) continue;
        groups.push_back(group);
        int y=270+row*145;
        fillRoundRect(r,370,y,1480,124,20,PANEL2);
        fillRoundRect(r,390,y+23,74,74,18,jobs[i].update?ORANGE:CYAN);
        drawTextCentered(r,427,y+48,jobs[i].update?"UP":"PKG",15,BG);
        drawText(r,492,y+18,jobs[i].name,18,WHITE);
        drawText(r,492,y+56,group==" "? "ID NAO IDENTIFICADO":group,12,MUTED);
        drawText(r,492,y+86,jobs[i].state,12,MINT);

        int sub=0;
        for(size_t k=0;k<jobs.size();++k) {
            if(k==i) continue;
            std::string other=jobs[k].titleId.empty()?jobs[k].name:jobs[k].titleId;
            if(other==group && jobs[k].update) {
                drawText(r,1270,y+35,"ATUALIZACAO",12,ORANGE);
                ++sub;
            }
        }
        if(sub>0) drawText(r,1270,y+63,"+"+std::to_string(sub)+" UPDATE(S)",12,MUTED);
        ++row;
    }
}

static void drawConnect(SDL_Renderer* r)
{
    fillRoundRect(r,330,126,1568,920,26,BG);
    drawText(r,370,164,"CONECTAR AO CELULAR",28,WHITE);
    drawText(r,370,218,"ESCANEIE ESTE QR CODE COM A CAMERA DO CELULAR.",15,MUTED);

    fillRoundRect(r,370,280,820,640,24,PANEL2);
    drawText(r,415,324,"1. CONECTE O PS4 E O CELULAR NA MESMA REDE WI-FI",15,WHITE);
    drawText(r,415,379,"2. ESCANEIE O QR CODE AO LADO",15,WHITE);
    drawText(r,415,434,"3. COLE O LINK DIRETO DO ARQUIVO PKG",15,WHITE);
    drawText(r,415,489,"4. TOQUE EM ENVIAR PARA INSTALAR",15,WHITE);
    fillRoundRect(r,415,576,730,2,2,BORDER);
    drawText(r,415,619,"ENDERECO DO PS4",13,MUTED);
    drawText(r,415,660,gIpAddress+":"+std::to_string(PHONE_PORT),20,CYAN);
    drawText(r,415,729,gServerReady.load()?"SERVIDOR DE PAREAMENTO ATIVO":"SERVIDOR AGUARDANDO REDE",15,gServerReady.load()?MINT:ORANGE);
    drawText(r,415,780,"A CHAVE DE PAREAMENTO JA ESTA DENTRO DO QR.",13,MUTED);
    drawText(r,415,830,"NAO PRECISA DIGITAR TOKEN NO CELULAR.",13,MUTED);

    fillRoundRect(r,1250,250,550,610,26,PANEL);
    drawTextCentered(r,1525,280,"ESCANEIE PARA CONECTAR",15,MINT);
    if(gQrCode) drawQrCode(r,1350,340,350);
    else drawTextCentered(r,1525,500,"QR INDISPONIVEL",18,ORANGE);
    drawTextCentered(r,1525,786,gOnline?"ABRA A PAGINA NEXUS STORE":"CONECTE O CONSOLE A WI-FI",13,MUTED);
}

static void render(SDL_Renderer* r)
{
    setColor(r,BG);
    SDL_RenderClear(r);
    drawHeader(r);
    drawSidebar(r);
    if(gSelectedNav==0) drawHome(r);
    else if(gSelectedNav==3) drawDownloads(r);
    else if(gSelectedNav==4) drawConnect(r);
    else drawGenericPage(r,gSelectedNav);
    fillRoundRect(r,330,1018,1568,38,16,PANEL);
    drawText(r,355,1028,"D-PAD  NAVEGAR",12,MUTED);
    drawText(r,610,1028,"X  SELECIONAR",12,WHITE);
    drawText(r,860,1028,"O  VOLTAR",12,WHITE);
    drawText(r,1540,1028,"NEXUS STORE",12,CYAN);
}

static void handleNavigation(SDL_Event& e, bool& running)
{
    if(e.type==SDL_QUIT) { running=false; return; }
    if(e.type==SDL_KEYDOWN && e.key.repeat==0) {
        SDL_Keycode key=e.key.keysym.sym;
        if(key==SDLK_UP) gSelectedNav=(gSelectedNav+NAV_COUNT-1)%NAV_COUNT;
        else if(key==SDLK_DOWN) gSelectedNav=(gSelectedNav+1)%NAV_COUNT;
        else if(key==SDLK_LEFT) gSelectedNav=(gSelectedNav+NAV_COUNT-1)%NAV_COUNT;
        else if(key==SDLK_RIGHT) gSelectedNav=(gSelectedNav+1)%NAV_COUNT;
        else if(key==SDLK_ESCAPE) gSelectedNav=0;
        else if(key==SDLK_RETURN) {
            if(gSelectedNav==4 && !gServerReady.load()) gLastMessage="SERVIDOR AINDA NAO ESTA PRONTO";
        }
    }
    if(e.type==SDL_JOYHATMOTION) {
        if(e.jhat.value & SDL_HAT_UP) gSelectedNav=(gSelectedNav+NAV_COUNT-1)%NAV_COUNT;
        else if(e.jhat.value & SDL_HAT_DOWN) gSelectedNav=(gSelectedNav+1)%NAV_COUNT;
        if(e.jhat.value & SDL_HAT_LEFT) gSelectedNav=(gSelectedNav+NAV_COUNT-1)%NAV_COUNT;
        else if(e.jhat.value & SDL_HAT_RIGHT) gSelectedNav=(gSelectedNav+1)%NAV_COUNT;
    }
    if(e.type==SDL_JOYBUTTONDOWN) {
        if(e.jbutton.button==1) gSelectedNav=0; // circle
        else if(e.jbutton.button==4) gSelectedNav=(gSelectedNav+NAV_COUNT-1)%NAV_COUNT; // L1
        else if(e.jbutton.button==5) gSelectedNav=(gSelectedNav+1)%NAV_COUNT; // R1
    }
}

int main(int argc,char* argv[])
{
    (void)argc; (void)argv;
    setvbuf(stdout,NULL,_IONBF,0);

    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK)!=0) return 1;
    SDL_JoystickEventState(SDL_ENABLE);

    if(sceSysmoduleLoadModule(0x009A)<0) {
        std::printf("FreeType module load failed\n");
    } else if(FT_Init_FreeType(&gFtLibrary)==0) {
        if(FT_New_Face(gFtLibrary,"/app0/assets/Inter.ttf",0,&gFont)!=0)
            std::printf("Unable to load Inter font\n");
    }

    gWindow=SDL_CreateWindow("NEXUS STORE",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,SCREEN_W,SCREEN_H,0);
    if(!gWindow) { SDL_Quit(); return 2; }
    SDL_Surface* windowSurface=SDL_GetWindowSurface(gWindow);
    gRenderer=SDL_CreateSoftwareRenderer(windowSurface);
    if(!gRenderer) { SDL_DestroyWindow(gWindow); SDL_Quit(); return 3; }

    if(SDL_NumJoysticks()>0) gController=SDL_JoystickOpen(0);

    gPairKey="";
    refreshNetworkAddress(true);
    createPairingQr();
    loadJobs();

    gStopServer=false;
    gServerThread=std::thread(phoneServerLoop);

    bool running=true;
    while(running) {
        SDL_Event e;
        while(SDL_PollEvent(&e)) handleNavigation(e,running);
        refreshNetworkAddress(false);
        render(gRenderer);
        SDL_UpdateWindowSurface(gWindow);
        SDL_Delay(16);
    }

    gStopServer=true;
    if(gServerSocket>=0) shutdown(gServerSocket,SHUT_RDWR);
    if(gServerThread.joinable()) gServerThread.join();
    if(gController) SDL_JoystickClose(gController);
    if(gQrCode) delete gQrCode;
    for(size_t i=0;i<gTextCache.size();++i)
        if(gTextCache[i].texture) SDL_DestroyTexture(gTextCache[i].texture);
    if(gFont) FT_Done_Face(gFont);
    if(gFtLibrary) FT_Done_FreeType(gFtLibrary);
    if(gRenderer) SDL_DestroyRenderer(gRenderer);
    if(gWindow) SDL_DestroyWindow(gWindow);
    SDL_Quit();
    return 0;
}
