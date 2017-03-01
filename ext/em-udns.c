#include <ruby.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "udns.h"
#include "em-udns.h"


static VALUE mEm;
static VALUE mUdns;

static VALUE cResolver;
static VALUE cQuery;

static VALUE cRR_MX;
static VALUE cRR_SRV;
static VALUE cRR_NAPTR;

static VALUE eUdnsError;

static ID id_timer;
static ID id_queries;
static ID id_domain;
static ID id_priority;
static ID id_weight;
static ID id_port;
static ID id_order;
static ID id_preference;
static ID id_flags;
static ID id_service;
static ID id_regexp;
static ID id_replacement;

static VALUE symbol_dns_error_tempfail;
static VALUE symbol_dns_error_protocol;
static VALUE symbol_dns_error_nxdomain;
static VALUE symbol_dns_error_nodata;
static VALUE symbol_dns_error_unknown;
static VALUE symbol_dns_error_badquery;
static VALUE symbol_dns_error_nomem;
static VALUE symbol_dns_error_unknown;

static ID method_cancel;
static ID method_set_timer;
static ID method_do_success;
static ID method_do_error;


void Resolver_free(struct dns_ctx *dns_context)
{
  if(dns_context)
    dns_free(dns_context);
}


VALUE Resolver_alloc(VALUE klass)
{
  struct dns_ctx *dns_context;
  VALUE alloc_error = Qnil;
  VALUE obj;

  /* First initialize the library (so the default context). */
  if (dns_init(NULL, 0) < 0)
    alloc_error = rb_str_new2("udns `dns_init' failed");

  /* Copy the context to a new one. */
  if (!(dns_context = dns_new(NULL)))
    alloc_error = rb_str_new2("udns `dns_new' failed");
  
  obj = Data_Wrap_Struct(klass, NULL, Resolver_free, dns_context);
  if (TYPE(alloc_error) == T_STRING)
    rb_ivar_set(obj, rb_intern("@alloc_error"), alloc_error);

  return obj;
}


void timer_cb(struct dns_ctx *dns_context, int timeout, void *data)
{
  VALUE resolver;
  VALUE timer;

  resolver = (VALUE)data;
  timer = rb_ivar_get(resolver, id_timer);

  /* Cancel the EM::Timer. */
  if (TYPE(timer) != T_NIL)
    rb_funcall(timer, method_cancel, 0);
  
  if (timeout >= 0)
    rb_funcall(resolver, method_set_timer, 1, INT2FIX(timeout));
}


VALUE Resolver_dns_open(VALUE self)
{
  struct dns_ctx *dns_context = NULL;

  Data_Get_Struct(self, struct dns_ctx, dns_context);

  dns_set_tmcbck(dns_context, timer_cb, (void*)self);
  
  /* Open the new context. */
  if (dns_open(dns_context) < 0)
    rb_raise(eUdnsError, "udns `dns_open' failed");

  return Qtrue;
}


VALUE Resolver_fd(VALUE self)
{
  struct dns_ctx *dns_context = NULL;

  Data_Get_Struct(self, struct dns_ctx, dns_context);
  return INT2FIX(dns_sock(dns_context));
}


VALUE Resolver_ioevent(VALUE self)
{
  struct dns_ctx *dns_context = NULL;
    
  Data_Get_Struct(self, struct dns_ctx, dns_context);
  dns_ioevent(dns_context, 0);
  return Qfalse;
}


VALUE Resolver_timeouts(VALUE self)
{
  struct dns_ctx *dns_context = NULL;
  
  Data_Get_Struct(self, struct dns_ctx, dns_context);
  dns_timeouts(dns_context, -1, 0);

  return Qnil;
}


VALUE Resolver_cancel(VALUE self, VALUE query)
{
  VALUE queries;

  queries = rb_ivar_get(self, id_queries);
  if (TYPE(rb_hash_aref(queries, query)) == T_TRUE) {
    rb_hash_aset(queries, query, Qfalse);
    return Qtrue;
  }
  else
    return Qfalse;
}


VALUE Resolver_active(VALUE self)
{
  struct dns_ctx *dns_context = NULL;

  Data_Get_Struct(self, struct dns_ctx, dns_context);
  return INT2FIX(dns_active(dns_context));
}


static void* check_query(struct dns_ctx *dns_context, void *rr, void *data)
{
  VALUE resolver;
  VALUE query;
  VALUE query_value_in_hash;
  VALUE error;
  int status;

  resolver = ((struct resolver_query*)data)->resolver;
  query    = ((struct resolver_query*)data)->query;
  xfree(data);

  query_value_in_hash = rb_hash_delete(rb_ivar_get(resolver, id_queries), query);
  /* Got response belongs to a query already removed (shouldn't occur). Ignore. */
  if (query_value_in_hash == Qnil) {
    if (rr) free(rr);
    return NULL;
  }
  /* Got response belongs to a cancelled query. Ignore. */
  else if (query_value_in_hash == Qfalse) {
    if (rr) free(rr);
    return NULL;
  }
  
  if ((status = dns_status(dns_context)) < 0) {
    if (rr) free(rr);
    switch(status) {
      case DNS_E_TEMPFAIL:
        error = symbol_dns_error_tempfail;
        break;
      case DNS_E_PROTOCOL:
        error = symbol_dns_error_protocol;
        break;
      case DNS_E_NXDOMAIN:
        error = symbol_dns_error_nxdomain;
        break;
      case DNS_E_NODATA:
        error = symbol_dns_error_nodata;
        break;
      default:
        error = symbol_dns_error_unknown;
        break;
    }
    rb_funcall(query, method_do_error, 1, error);
    return NULL;
  }

  return (void*)query;
}


static void dns_result_A_cb(struct dns_ctx *dns_context, struct dns_rr_a4 *rr, void *data)
{
  VALUE query;
  VALUE array;
  int i;
  char ip[INET_ADDRSTRLEN];
  
  if (!(query = (VALUE)check_query(dns_context, rr, data)))  return;
  
  array = rb_ary_new2(rr->dnsa4_nrr);
  for(i = 0; i < rr->dnsa4_nrr; i++)
    rb_ary_push(array, rb_str_new2((char *)dns_ntop(AF_INET, &(rr->dnsa4_addr[i].s_addr), ip, INET_ADDRSTRLEN)));
  free(rr);

  rb_funcall(query, method_do_success, 1, array);
}


static void dns_result_AAAA_cb(struct dns_ctx *dns_context, struct dns_rr_a6 *rr, void *data)
{
  VALUE query;
  VALUE array;
  int i;
  char ip[INET6_ADDRSTRLEN];

  if (!(query = (VALUE)check_query(dns_context, rr, data)))  return;

  array = rb_ary_new2(rr->dnsa6_nrr);
  for(i = 0; i < rr->dnsa6_nrr; i++)
    rb_ary_push(array, rb_str_new2((char *)dns_ntop(AF_INET6, &(rr->dnsa6_addr[i].s6_addr), ip, INET6_ADDRSTRLEN)));
  free(rr);

  rb_funcall(query, method_do_success, 1, array);
}


static void dns_result_PTR_cb(struct dns_ctx *dns_context, struct dns_rr_ptr *rr, void *data)
{
  VALUE query;
  VALUE array;
  int i;
  
  if (!(query = (VALUE)check_query(dns_context, rr, data)))  return;

  array = rb_ary_new2(rr->dnsptr_nrr);
  for(i = 0; i < rr->dnsptr_nrr; i++)
    rb_ary_push(array, rb_str_new2(rr->dnsptr_ptr[i]));
  free(rr);

  rb_funcall(query, method_do_success, 1, array);
}


static void dns_result_MX_cb(struct dns_ctx *dns_context, struct dns_rr_mx *rr, void *data)
{
  VALUE query;
  VALUE array;
  int i;
  VALUE rr_mx;

  if (!(query = (VALUE)check_query(dns_context, rr, data)))  return;

  array = rb_ary_new2(rr->dnsmx_nrr);
  for(i = 0; i < rr->dnsmx_nrr; i++) {
    rr_mx = rb_obj_alloc(cRR_MX);
    rb_ivar_set(rr_mx, id_domain, rb_str_new2(rr->dnsmx_mx[i].name));
    rb_ivar_set(rr_mx, id_priority, INT2FIX(rr->dnsmx_mx[i].priority));
    rb_ary_push(array, rr_mx);
  }
  free(rr);

  rb_funcall(query, method_do_success, 1, array);
}

static void dns_result_NS_cb(struct dns_ctx *dns_context, struct dns_rr_ns *rr, void *data)
{
  VALUE query;
  VALUE array;
  int i;

  if (!(query = (VALUE)check_query(dns_context, rr, data))) return;

  array = rb_ary_new2(rr->dnsns_nrr);
  for(i = 0; i < rr->dnsns_nrr; i++) {
    rb_ary_push(array, rb_str_new2(rr->dnsns_ns[i]));
  }
  free(rr);

  rb_funcall(query, method_do_success, 1, array);
}



static void dns_result_TXT_cb(struct dns_ctx *dns_context, struct dns_rr_txt *rr, void *data)
{
  VALUE query;
  VALUE array;
  int i;

  if (!(query = (VALUE)check_query(dns_context, rr, data)))  return;

  array = rb_ary_new2(rr->dnstxt_nrr);
  for(i = 0; i < rr->dnstxt_nrr; i++)
    rb_ary_push(array, rb_str_new((const char*)rr->dnstxt_txt[i].txt, rr->dnstxt_txt[i].len));
  free(rr);

  rb_funcall(query, method_do_success, 1, array);
}


static void dns_result_SRV_cb(struct dns_ctx *dns_context, struct dns_rr_srv *rr, void *data)
{
  VALUE query;
  VALUE array;
  int i;
  VALUE rr_srv;

  if (!(query = (VALUE)check_query(dns_context, rr, data)))  return;

  array = rb_ary_new2(rr->dnssrv_nrr);
  for(i = 0; i < rr->dnssrv_nrr; i++) {
    rr_srv = rb_obj_alloc(cRR_SRV);
    rb_ivar_set(rr_srv, id_domain, rb_str_new2(rr->dnssrv_srv[i].name));
    rb_ivar_set(rr_srv, id_priority, INT2FIX(rr->dnssrv_srv[i].priority));
    rb_ivar_set(rr_srv, id_weight, INT2FIX(rr->dnssrv_srv[i].weight));
    rb_ivar_set(rr_srv, id_port, INT2FIX(rr->dnssrv_srv[i].port));
    rb_ary_push(array, rr_srv);
  }
  free(rr);

  rb_funcall(query, method_do_success, 1, array);
}


static void dns_result_NAPTR_cb(struct dns_ctx *dns_context, struct dns_rr_naptr *rr, void *data)
{
  VALUE query;
  VALUE array;
  int i;
  VALUE rr_naptr;

  if (!(query = (VALUE)check_query(dns_context, rr, data)))  return;

  array = rb_ary_new2(rr->dnsnaptr_nrr);
  for(i = 0; i < rr->dnsnaptr_nrr; i++) {
    rr_naptr = rb_obj_alloc(cRR_NAPTR);
    rb_ivar_set(rr_naptr, id_order, INT2FIX(rr->dnsnaptr_naptr[i].order));
    rb_ivar_set(rr_naptr, id_preference, INT2FIX(rr->dnsnaptr_naptr[i].preference));
    rb_ivar_set(rr_naptr, id_flags, rb_str_new2(rr->dnsnaptr_naptr[i].flags));
    rb_ivar_set(rr_naptr, id_service, rb_str_new2(rr->dnsnaptr_naptr[i].service));
    if (strlen(rr->dnsnaptr_naptr[i].regexp) > 0)
      rb_ivar_set(rr_naptr, id_regexp, rb_str_new2(rr->dnsnaptr_naptr[i].regexp));
    else
      rb_ivar_set(rr_naptr, id_regexp, Qnil);
    if (strlen(rr->dnsnaptr_naptr[i].replacement) > 0)
      rb_ivar_set(rr_naptr, id_replacement, rb_str_new2(rr->dnsnaptr_naptr[i].replacement));
    else
      rb_ivar_set(rr_naptr, id_replacement, Qnil);
    rb_ary_push(array, rr_naptr);
  }
  free(rr);

  rb_funcall(query, method_do_success, 1, array);
}


VALUE get_dns_error(struct dns_ctx *dns_context)
{
  switch(dns_status(dns_context)) {
    case DNS_E_TEMPFAIL:
      return symbol_dns_error_tempfail;
      break;
    case DNS_E_NOMEM:
      return symbol_dns_error_nomem;
      break;
    case DNS_E_BADQUERY:
      return symbol_dns_error_badquery;
      break;
    default:
      return symbol_dns_error_unknown;
      break;
  }
}


VALUE Resolver_submit_A(VALUE self, VALUE rb_domain)
{
  struct dns_ctx *dns_context;
  char *domain;
  VALUE query;
  VALUE error;
  struct resolver_query *data;

  
  Data_Get_Struct(self, struct dns_ctx, dns_context);
  domain = StringValueCStr(rb_domain);
  query = rb_obj_alloc(cQuery);

  data = ALLOC(struct resolver_query);
  data->resolver = self;
  data->query = query;
  
  if (!dns_submit_a4(dns_context, domain, 0, dns_result_A_cb, (void *)data)) {
    error = get_dns_error(dns_context);
    xfree(data);
    rb_funcall(query, method_do_error, 1, error);
  }
  else {
    rb_hash_aset(rb_ivar_get(self, id_queries), query, Qtrue);
  }
  
  return query;
}


VALUE Resolver_submit_AAAA(VALUE self, VALUE rb_domain)
{
  struct dns_ctx *dns_context;
  char *domain;
  VALUE query;
  VALUE error;
  struct resolver_query *data;

  Data_Get_Struct(self, struct dns_ctx, dns_context);
  domain = StringValueCStr(rb_domain);
  query = rb_obj_alloc(cQuery);

  data = ALLOC(struct resolver_query);
  data->resolver = self;
  data->query = query;

  if (!dns_submit_a6(dns_context, domain, 0, dns_result_AAAA_cb, (void *)data)) {
    error = get_dns_error(dns_context);
    xfree(data);
    rb_funcall(query, method_do_error, 1, error);
  }
  else {
    rb_hash_aset(rb_ivar_get(self, id_queries), query, Qtrue);
  }
  
  return query;
}


VALUE Resolver_submit_PTR(VALUE self, VALUE rb_ip)
{
  struct dns_ctx *dns_context;
  char *ip;
  VALUE query;
  VALUE error;
  struct resolver_query *data;
  struct in_addr addr;
  struct in6_addr addr6;

  Data_Get_Struct(self, struct dns_ctx, dns_context);
  ip = StringValueCStr(rb_ip);
  query = rb_obj_alloc(cQuery);

  data = ALLOC(struct resolver_query);
  data->resolver = self;
  data->query = query;

  switch(dns_pton(AF_INET, ip, &addr)) {
    /* It's valid IPv4. */
    case 1:
      if (!dns_submit_a4ptr(dns_context, &addr, dns_result_PTR_cb, (void *)data)) {
        error = get_dns_error(dns_context);
        xfree(data);
        rb_funcall(query, method_do_error, 1, error);
      }
      else {
        rb_hash_aset(rb_ivar_get(self, id_queries), query, Qtrue);
      }
      break;
    /* Invalid IPv4, let's try with IPv6. */
    case 0:
      switch(dns_pton(AF_INET6, ip, &addr6)) {
        /* It's valid IPv6. */
        case 1:
          if (!dns_submit_a6ptr(dns_context, &addr6, dns_result_PTR_cb, (void *)data)) {
            error = get_dns_error(dns_context);
            xfree(data);
            rb_funcall(query, method_do_error, 1, error);
          }
          else {
            rb_hash_aset(rb_ivar_get(self, id_queries), query, Qtrue);
          }
          break;
        /* Also an invalid IPv6 so the IP is invalid. */
        case 0:
          xfree(data);
          rb_funcall(query, method_do_error, 1, symbol_dns_error_badquery);
          break;
      }
      break;
  }

  return query;
}


VALUE Resolver_submit_MX(VALUE self, VALUE rb_domain)
{
  struct dns_ctx *dns_context;
  char *domain;
  VALUE query;
  VALUE error;
  struct resolver_query *data;

  Data_Get_Struct(self, struct dns_ctx, dns_context);
  domain = StringValueCStr(rb_domain);
  query = rb_obj_alloc(cQuery);

  data = ALLOC(struct resolver_query);
  data->resolver = self;
  data->query = query;

  if (!dns_submit_mx(dns_context, domain, 0, dns_result_MX_cb, (void *)data)) {
    error = get_dns_error(dns_context);
    xfree(data);
    rb_funcall(query, method_do_error, 1, error);
  }
  else {
    rb_hash_aset(rb_ivar_get(self, id_queries), query, Qtrue);
  }
  
  return query;
}

VALUE Resolver_submit_NS(VALUE self, VALUE rb_domain)
{
  struct dns_ctx *dns_context;
  char *domain;
  VALUE query;
  VALUE error;
  struct resolver_query *data;

  Data_Get_Struct(self, struct dns_ctx, dns_context);
  domain = StringValueCStr(rb_domain);
  query = rb_obj_alloc(cQuery);

  data = ALLOC(struct resolver_query);
  data->resolver = self;
  data-> query = query;

  if (!dns_submit_ns(dns_context, domain, 0, dns_result_NS_cb, (void *)data)) {
    error = get_dns_error(dns_context);
    xfree(data);
    rb_funcall(query, method_do_error, 1, error);
  }
  else {
    rb_hash_aset(rb_ivar_get(self, id_queries), query, Qtrue);
  }
  return query;
}


VALUE Resolver_submit_TXT(VALUE self, VALUE rb_domain)
{
  struct dns_ctx *dns_context;
  char *domain;
  VALUE query;
  VALUE error;
  struct resolver_query *data;

  Data_Get_Struct(self, struct dns_ctx, dns_context);
  domain = StringValueCStr(rb_domain);
  query = rb_obj_alloc(cQuery);

  data = ALLOC(struct resolver_query);
  data->resolver = self;
  data->query = query;

  if (!dns_submit_txt(dns_context, domain, DNS_C_IN, 0, dns_result_TXT_cb, (void *)data)) {
    error = get_dns_error(dns_context);
    xfree(data);
    rb_funcall(query, method_do_error, 1, error);
  }
  else {
    rb_hash_aset(rb_ivar_get(self, id_queries), query, Qtrue);
  }
  
  return query;
}


VALUE Resolver_submit_SRV(int argc, VALUE *argv, VALUE self)
{
  struct dns_ctx *dns_context;
  char *domain;
  char *service = NULL;
  char *protocol = NULL;
  VALUE query;
  VALUE error;
  struct resolver_query *data;

  if (argc == 1 && TYPE(argv[0]) == T_STRING);
  else if (argc == 3 && TYPE(argv[0]) == T_STRING &&
           TYPE(argv[1]) == T_STRING && TYPE(argv[2]) == T_STRING) {
    service = StringValueCStr(argv[1]);
    protocol = StringValueCStr(argv[2]);
  }
  else if (argc == 3 && TYPE(argv[0]) == T_STRING &&
           TYPE(argv[1]) == T_NIL && TYPE(argv[2]) == T_NIL);
  else
    rb_raise(rb_eArgError, "arguments must be `domain' or `domain',`service',`protocol'");

  Data_Get_Struct(self, struct dns_ctx, dns_context);
  domain = StringValueCStr(argv[0]);
  query = rb_obj_alloc(cQuery);
  
  data = ALLOC(struct resolver_query);
  data->resolver = self;
  data->query = query;
  
  if (!dns_submit_srv(dns_context, domain, service, protocol, 0, dns_result_SRV_cb, (void *)data)) {
    error = get_dns_error(dns_context);
    xfree(data);
    rb_funcall(query, method_do_error, 1, error);
  }
  else {
    rb_hash_aset(rb_ivar_get(self, id_queries), query, Qtrue);
  }
  
  return query;
}


VALUE Resolver_submit_NAPTR(VALUE self, VALUE rb_domain)
{
  struct dns_ctx *dns_context;
  char *domain;
  VALUE query;
  VALUE error;
  struct resolver_query *data;

  Data_Get_Struct(self, struct dns_ctx, dns_context);
  domain = StringValueCStr(rb_domain);
  query = rb_obj_alloc(cQuery);

  data = ALLOC(struct resolver_query);
  data->resolver = self;
  data->query = query;

  if (!dns_submit_naptr(dns_context, domain, 0, dns_result_NAPTR_cb, (void *)data)) {
    error = get_dns_error(dns_context);
    xfree(data);
    rb_funcall(query, method_do_error, 1, error);
  }
  else {
    rb_hash_aset(rb_ivar_get(self, id_queries), query, Qtrue);
  }
  
  return query;
}

int _add_serv_s(struct dns_ctx *dns_context, const char *ip, in_port_t port)
{
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  inet_aton(ip, &server_addr.sin_addr);
  server_addr.sin_port = htons(port);
  return dns_add_serv_s(dns_context, (struct sockaddr *)&server_addr);
}

VALUE Resolver_add_serv(VALUE self, VALUE ip)
{
  struct dns_ctx *dns_context;
  struct servent *sp;

  Data_Get_Struct(self, struct dns_ctx, dns_context);

  if (TYPE(ip) == T_NIL) {
    return INT2FIX(dns_add_serv(dns_context, NULL));
  }

  sp = getservbyname("domain", "udp");
  return INT2FIX(_add_serv_s(dns_context, StringValueCStr(ip), htons(sp->s_port)));
}

VALUE Resolver_add_serv_s(VALUE self, VALUE ip, VALUE port)
{
  struct dns_ctx *dns_context;
  Data_Get_Struct(self, struct dns_ctx, dns_context);
  return INT2FIX(_add_serv_s(dns_context, StringValueCStr(ip), FIX2INT(port)));
}

/* Attribute readers. */
VALUE RR_MX_domain(VALUE self)          { return rb_ivar_get(self, id_domain); }
VALUE RR_MX_priority(VALUE self)        { return rb_ivar_get(self, id_priority); }

VALUE RR_SRV_priority(VALUE self)       { return rb_ivar_get(self, id_priority); }
VALUE RR_SRV_weight(VALUE self)         { return rb_ivar_get(self, id_weight); }
VALUE RR_SRV_port(VALUE self)           { return rb_ivar_get(self, id_port); }
VALUE RR_SRV_domain(VALUE self)         { return rb_ivar_get(self, id_domain); }

VALUE RR_NAPTR_order(VALUE self)        { return rb_ivar_get(self, id_order); }
VALUE RR_NAPTR_preference(VALUE self)   { return rb_ivar_get(self, id_preference); }
VALUE RR_NAPTR_flags(VALUE self)        { return rb_ivar_get(self, id_flags); }
VALUE RR_NAPTR_service(VALUE self)      { return rb_ivar_get(self, id_service); }
VALUE RR_NAPTR_regexp(VALUE self)       { return rb_ivar_get(self, id_regexp); }
VALUE RR_NAPTR_replacement(VALUE self)  { return rb_ivar_get(self, id_replacement); }


void Init_em_udns_ext()
{
  mEm = rb_define_module("EventMachine");
  mUdns = rb_define_module_under(mEm, "Udns");
  eUdnsError = rb_define_class_under(mUdns, "UdnsError", rb_eStandardError);

  cResolver = rb_define_class_under(mUdns, "Resolver", rb_cObject);
  rb_define_alloc_func(cResolver, Resolver_alloc);
  rb_define_private_method(cResolver, "dns_open", Resolver_dns_open, 0);
  rb_define_method(cResolver, "fd", Resolver_fd, 0);
  rb_define_method(cResolver, "ioevent", Resolver_ioevent, 0);
  rb_define_private_method(cResolver, "timeouts", Resolver_timeouts, 0);
  rb_define_method(cResolver, "active", Resolver_active, 0);
  rb_define_method(cResolver, "cancel", Resolver_cancel, 1);
  rb_define_method(cResolver, "submit_A", Resolver_submit_A, 1);
  rb_define_method(cResolver, "submit_AAAA", Resolver_submit_AAAA, 1);
  rb_define_method(cResolver, "submit_PTR", Resolver_submit_PTR, 1);
  rb_define_method(cResolver, "submit_MX", Resolver_submit_MX, 1);
  rb_define_method(cResolver, "submit_TXT", Resolver_submit_TXT, 1);
  rb_define_method(cResolver, "submit_SRV", Resolver_submit_SRV, -1);
  rb_define_method(cResolver, "submit_NAPTR", Resolver_submit_NAPTR, 1);
  rb_define_method(cResolver, "submit_NS", Resolver_submit_NS, 1);
  rb_define_method(cResolver, "add_serv", Resolver_add_serv, 1);
  rb_define_method(cResolver, "add_serv_s", Resolver_add_serv_s, 2);

  cQuery = rb_define_class_under(mUdns, "Query", rb_cObject);

  cRR_MX = rb_define_class_under(mUdns, "RR_MX", rb_cObject);
  rb_define_method(cRR_MX, "domain", RR_MX_domain, 0);
  rb_define_method(cRR_MX, "priority", RR_MX_priority, 0);

  cRR_SRV = rb_define_class_under(mUdns, "RR_SRV", rb_cObject);
  rb_define_method(cRR_SRV, "priority", RR_SRV_priority, 0);
  rb_define_method(cRR_SRV, "weight", RR_SRV_weight, 0);
  rb_define_method(cRR_SRV, "port", RR_SRV_port, 0);
  rb_define_method(cRR_SRV, "domain", RR_SRV_domain, 0);

  cRR_NAPTR = rb_define_class_under(mUdns, "RR_NAPTR", rb_cObject);
  rb_define_method(cRR_NAPTR, "order", RR_NAPTR_order, 0);
  rb_define_method(cRR_NAPTR, "preference", RR_NAPTR_preference, 0);
  rb_define_method(cRR_NAPTR, "flags", RR_NAPTR_flags, 0);
  rb_define_method(cRR_NAPTR, "service", RR_NAPTR_service, 0);
  rb_define_method(cRR_NAPTR, "regexp", RR_NAPTR_regexp, 0);
  rb_define_method(cRR_NAPTR, "replacement", RR_NAPTR_replacement, 0);

  id_timer = rb_intern("@timer");
  id_queries = rb_intern("@queries");
  id_domain = rb_intern("@domain");
  id_priority = rb_intern("@priority");
  id_weight = rb_intern("@weight");
  id_port = rb_intern("@port");
  id_order = rb_intern("@order");
  id_preference = rb_intern("@preference");
  id_flags = rb_intern("@flags");
  id_service = rb_intern("@service");
  id_regexp = rb_intern("@regexp");
  id_replacement = rb_intern("@replacement");

  symbol_dns_error_tempfail = ID2SYM(rb_intern("dns_error_tempfail"));
  symbol_dns_error_protocol = ID2SYM(rb_intern("dns_error_protocol"));
  symbol_dns_error_nxdomain = ID2SYM(rb_intern("dns_error_nxdomain"));
  symbol_dns_error_nodata = ID2SYM(rb_intern("dns_error_nodata"));
  symbol_dns_error_unknown = ID2SYM(rb_intern("dns_error_unknown"));
  symbol_dns_error_badquery = ID2SYM(rb_intern("dns_error_badquery"));
  symbol_dns_error_nomem = ID2SYM(rb_intern("dns_error_nomem"));
  symbol_dns_error_unknown = ID2SYM(rb_intern("dns_error_unknown"));

  method_cancel = rb_intern("cancel");
  method_set_timer = rb_intern("set_timer");
  method_do_success = rb_intern("do_success");
  method_do_error = rb_intern("do_error");
}
