// WiFiClientSecure for the emulator (emu/stubs/WiFiClientSecure.h): TCP socket + OpenSSL, verification against
// the CA PEM the firmware passes (the same roots the device trusts), host name check, SNI, handshake timeout.
#include <WiFiClientSecure.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <string>
#include <vector>
struct WiFiClientSecure::Tls {
 int fd=-1;SSL_CTX *ctx=nullptr;SSL *ssl=nullptr;std::vector<uint8_t> rx;bool closed=false;
 ~Tls(){if(ssl){SSL_shutdown(ssl);SSL_free(ssl);}if(ctx)SSL_CTX_free(ctx);if(fd>=0)close(fd);}
 // Pulls whatever TLS records are ready into rx without blocking.
 void fill(){
  if(!ssl||closed)return;uint8_t buf[4096];
  while(true){int n=SSL_read(ssl,buf,sizeof(buf));if(n>0){rx.insert(rx.end(),buf,buf+n);continue;}
   int e=SSL_get_error(ssl,n);if(e==SSL_ERROR_WANT_READ||e==SSL_ERROR_WANT_WRITE)return;closed=true;return;}
 }
};
static std::string lastTlsError;
static bool waitFd(int fd,short events,int ms){pollfd p{fd,events,0};return poll(&p,1,ms)>0;}
WiFiClientSecure::WiFiClientSecure(){}
WiFiClientSecure::~WiFiClientSecure(){}
int WiFiClientSecure::connect(const char *host,uint16_t port){return connect(host,port,30000);}
int WiFiClientSecure::connect(IPAddress ip,uint16_t port){return connect(ip.toString().c_str(),port,30000);}
int WiFiClientSecure::connect(const char *host,uint16_t port,int32_t timeoutMs){
 stop();if(WiFi.status()!=WL_CONNECTED){lastTlsError="Wi-Fi not connected";return 0;}
 if(!insecure&&!ca&&!bundle){lastTlsError="no CA certificate";return 0;}// as the ESP32 ssl_client: nothing to verify with
 addrinfo hints{},*res=nullptr;hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;
 if(getaddrinfo(host,std::to_string(port).c_str(),&hints,&res)!=0||!res){lastTlsError="DNS lookup failed";return 0;}
 auto t=std::make_shared<Tls>();t->fd=socket(AF_INET,SOCK_STREAM,0);if(t->fd<0){freeaddrinfo(res);return 0;}
 fcntl(t->fd,F_SETFL,fcntl(t->fd,F_GETFL)|O_NONBLOCK);
 int r=::connect(t->fd,res->ai_addr,res->ai_addrlen);freeaddrinfo(res);
 if(r<0&&errno==EINPROGRESS){int err=0;socklen_t len=sizeof(err);if(!waitFd(t->fd,POLLOUT,timeoutMs)||getsockopt(t->fd,SOL_SOCKET,SO_ERROR,&err,&len)<0||err){lastTlsError="TCP connect failed";return 0;}}
 else if(r<0){lastTlsError="TCP connect failed";return 0;}
 t->ctx=SSL_CTX_new(TLS_client_method());if(!t->ctx)return 0;
 if(insecure)SSL_CTX_set_verify(t->ctx,SSL_VERIFY_NONE,nullptr);
 else if(bundle&&!ca){SSL_CTX_set_verify(t->ctx,SSL_VERIFY_PEER,nullptr);SSL_CTX_set_default_verify_paths(t->ctx);}
 else{X509_STORE *store=SSL_CTX_get_cert_store(t->ctx);BIO *bio=BIO_new_mem_buf(ca,-1);int added=0;
  while(X509 *cert=PEM_read_bio_X509(bio,nullptr,nullptr,nullptr)){if(X509_STORE_add_cert(store,cert))added++;X509_free(cert);}
  BIO_free(bio);ERR_clear_error();if(!added){lastTlsError="CA certificate unreadable";return 0;}
  SSL_CTX_set_verify(t->ctx,SSL_VERIFY_PEER,nullptr);}
 t->ssl=SSL_new(t->ctx);SSL_set_fd(t->ssl,t->fd);SSL_set_tlsext_host_name(t->ssl,host);
 if(!insecure){X509_VERIFY_PARAM *param=SSL_get0_param(t->ssl);X509_VERIFY_PARAM_set1_host(param,host,0);}
 uint32_t deadline=millis()+uint32_t(handshakeSeconds*1000);
 while(true){int h=SSL_connect(t->ssl);if(h==1)break;int e=SSL_get_error(t->ssl,h);
  if((e==SSL_ERROR_WANT_READ||e==SSL_ERROR_WANT_WRITE)&&int32_t(deadline-millis())>0){waitFd(t->fd,e==SSL_ERROR_WANT_READ?POLLIN:POLLOUT,50);continue;}
  long v=SSL_get_verify_result(t->ssl);lastTlsError=v!=X509_V_OK?std::string("certificate rejected: ")+X509_verify_cert_error_string(v):"TLS handshake failed";
  fprintf(stderr,"[E][tls_emu] %s:%u %s\n",host,port,lastTlsError.c_str());return 0;}
 tls=t;return 1;
}
uint8_t WiFiClientSecure::connected(){if(!tls)return 0;tls->fill();return !tls->closed||!tls->rx.empty();}
void WiFiClientSecure::stop(){tls.reset();}
int WiFiClientSecure::available(){if(!tls)return 0;tls->fill();return int(tls->rx.size());}
int WiFiClientSecure::read(){uint8_t b;return read(&b,1)==1?b:-1;}
int WiFiClientSecure::read(uint8_t *buf,size_t n){if(!tls)return -1;tls->fill();if(tls->rx.empty())return -1;size_t k=std::min(n,tls->rx.size());memcpy(buf,tls->rx.data(),k);tls->rx.erase(tls->rx.begin(),tls->rx.begin()+k);return int(k);}
int WiFiClientSecure::peek(){if(!tls)return -1;tls->fill();return tls->rx.empty()?-1:tls->rx[0];}
size_t WiFiClientSecure::write(uint8_t b){return write(&b,1);}
size_t WiFiClientSecure::write(const uint8_t *buf,size_t n){
 if(!tls||!tls->ssl)return 0;size_t done=0;uint32_t start=millis();
 while(done<n&&millis()-start<5000){int w=SSL_write(tls->ssl,buf+done,int(n-done));if(w>0){done+=size_t(w);continue;}
  int e=SSL_get_error(tls->ssl,w);if(e==SSL_ERROR_WANT_WRITE||e==SSL_ERROR_WANT_READ){waitFd(tls->fd,POLLOUT,20);continue;}break;}
 return done;
}
int WiFiClientSecure::lastError(char *buf,size_t size){if(buf&&size)snprintf(buf,size,"%s",lastTlsError.c_str());return lastTlsError.empty()?0:-1;}
