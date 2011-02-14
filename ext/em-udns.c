#include <ruby.h>
#include <ctype.h>
#include <netinet/in.h>
#include "udns.h"
#include "em-udns.h"


static VALUE mEm;
static VALUE mEmDeferrable;
static VALUE mUdns;

static VALUE cResolver;
static VALUE cQuery;

static VALUE cRR_MX;
static VALUE cRR_SRV;
static VALUE cRR_NAPTR;

static VALUE eUdnsError;
static VALUE eUdnsTempFail;
static VALUE eUdnsNoMem;
static VALUE eUdnsBadQuery;


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
  timer = rb_ivar_get(resolver, rb_intern("@timer"));

  /* Cancel the EM::Timer. */
  if (TYPE(timer) != T_NIL)
    rb_funcall(timer, rb_intern("cancel"), 0);
  
  if (timeout >= 0)
    rb_funcall(resolver, rb_intern("set_timer"), 1, INT2FIX(timeout));
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
  VALUE value;
  
  queries = rb_ivar_get(self, rb_intern("@queries"));
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

  query_value_in_hash = rb_hash_delete(rb_ivar_get(resolver, rb_intern("@queries")), query);
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
        error = ID2SYM(rb_intern("dns_error_tempfail"));
        break;
      case DNS_E_PROTOCOL:
        error = ID2SYM(rb_intern("dns_error_protocol"));
        break;
      case DNS_E_NXDOMAIN:
        error = ID2SYM(rb_intern("dns_error_nxdomain"));
        break;
      case DNS_E_NODATA:
        error = ID2SYM(rb_intern("dns_error_nodata"));
        break;
      default:
        error = ID2SYM(rb_intern("dns_error_unknown"));
        break;
    }
    rb_funcall(query, rb_intern("set_deferred_status"), 2, ID2SYM(rb_intern("failed")), error);
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

  rb_funcall(query, rb_intern("set_deferred_status"), 2, ID2SYM(rb_intern("succeeded")), array);
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

  rb_funcall(query, rb_intern("set_deferred_status"), 2, ID2SYM(rb_intern("succeeded")), array);
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
  
  rb_funcall(query, rb_intern("set_deferred_status"), 2, ID2SYM(rb_intern("succeeded")), array);
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
    rb_ivar_set(rr_mx, rb_intern("@domain"), rb_str_new2(rr->dnsmx_mx[i].name));
    rb_ivar_set(rr_mx, rb_intern("@priority"), INT2FIX(rr->dnsmx_mx[i].priority));
    rb_ary_push(array, rr_mx);
  }
  free(rr);

  rb_funcall(query, rb_intern("set_deferred_status"), 2, ID2SYM(rb_intern("succeeded")), array);
}


static void dns_result_TXT_cb(struct dns_ctx *dns_context, struct dns_rr_txt *rr, void *data)
{
  VALUE query;
  VALUE array;
  int i;

  if (!(query = (VALUE)check_query(dns_context, rr, data)))  return;

  array = rb_ary_new2(rr->dnstxt_nrr);
  for(i = 0; i < rr->dnstxt_nrr; i++)
    rb_ary_push(array, rb_str_new(rr->dnstxt_txt[i].txt, rr->dnstxt_txt[i].len));
  free(rr);

  rb_funcall(query, rb_intern("set_deferred_status"), 2, ID2SYM(rb_intern("succeeded")), array);
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
    rb_ivar_set(rr_srv, rb_intern("@domain"), rb_str_new2(rr->dnssrv_srv[i].name));
    rb_ivar_set(rr_srv, rb_intern("@priority"), INT2FIX(rr->dnssrv_srv[i].priority));
    rb_ivar_set(rr_srv, rb_intern("@weight"), INT2FIX(rr->dnssrv_srv[i].weight));
    rb_ivar_set(rr_srv, rb_intern("@port"), INT2FIX(rr->dnssrv_srv[i].port));
    rb_ary_push(array, rr_srv);
  }
  free(rr);

  rb_funcall(query, rb_intern("set_deferred_status"), 2, ID2SYM(rb_intern("succeeded")), array);
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
    rb_ivar_set(rr_naptr, rb_intern("@order"), INT2FIX(rr->dnsnaptr_naptr[i].order));
    rb_ivar_set(rr_naptr, rb_intern("@preference"), INT2FIX(rr->dnsnaptr_naptr[i].preference));
    rb_ivar_set(rr_naptr, rb_intern("@flags"), rb_str_new2(rr->dnsnaptr_naptr[i].flags));
    rb_ivar_set(rr_naptr, rb_intern("@service"), rb_str_new2(rr->dnsnaptr_naptr[i].service));
    if (strlen(rr->dnsnaptr_naptr[i].regexp) > 0)
      rb_ivar_set(rr_naptr, rb_intern("@regexp"), rb_str_new2(rr->dnsnaptr_naptr[i].regexp));
    else
      rb_ivar_set(rr_naptr, rb_intern("@regexp"), Qnil);
    if (strlen(rr->dnsnaptr_naptr[i].replacement) > 0)
      rb_ivar_set(rr_naptr, rb_intern("@replacement"), rb_str_new2(rr->dnsnaptr_naptr[i].replacement));
    else
      rb_ivar_set(rr_naptr, rb_intern("@replacement"), Qnil);
    rb_ary_push(array, rr_naptr);
  }
  free(rr);

  rb_funcall(query, rb_intern("set_deferred_status"), 2, ID2SYM(rb_intern("succeeded")), array);
}


void raise_dns_error(struct dns_ctx *dns_context)
{
  switch(dns_status(dns_context)) {
    case DNS_E_TEMPFAIL:
      rb_raise(eUdnsTempFail, "internal error occured");
      break;
    case DNS_E_NOMEM:
      rb_raise(eUdnsNoMem, "no memory available to allocate query structure");
      break;
    case DNS_E_BADQUERY:
      rb_raise(eUdnsBadQuery, "name of dn is invalid");
      break;
    default:
      rb_raise(eUdnsError, "udns `dns_status' returns unexpected error %i", dns_status(dns_context));
      break;
  }
}


VALUE Resolver_submit_A(VALUE self, VALUE rb_domain)
{
  struct dns_ctx *dns_context;
  char *domain;
  VALUE query;
  struct resolver_query *data;
  
  Data_Get_Struct(self, struct dns_ctx, dns_context);
  domain = StringValueCStr(rb_domain);
  query = rb_obj_alloc(cQuery);

  data = ALLOC(struct resolver_query);
  data->resolver = self;
  data->query = query;
  
  if (!dns_submit_a4(dns_context, domain, 0, dns_result_A_cb, (void *)data)) {
    xfree(data);
    raise_dns_error(dns_context);
  }

  rb_hash_aset(rb_ivar_get(self, rb_intern("@queries")), query, Qtrue);
  return query;
}


VALUE Resolver_submit_AAAA(VALUE self, VALUE rb_domain)
{
  struct dns_ctx *dns_context;
  char *domain;
  VALUE query;
  struct resolver_query *data;

  Data_Get_Struct(self, struct dns_ctx, dns_context);
  domain = StringValueCStr(rb_domain);
  query = rb_obj_alloc(cQuery);

  data = ALLOC(struct resolver_query);
  data->resolver = self;
  data->query = query;

  if (!dns_submit_a6(dns_context, domain, 0, dns_result_AAAA_cb, (void *)data)) {
    xfree(data);
    raise_dns_error(dns_context);
  }

  rb_hash_aset(rb_ivar_get(self, rb_intern("@queries")), query, Qtrue);
  return query;
}


VALUE Resolver_submit_PTR(VALUE self, VALUE rb_ip)
{
  struct dns_ctx *dns_context;
  char *ip;
  VALUE query;
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
        xfree(data);
        raise_dns_error(dns_context);
      }
      break;
    /* Invalid IPv4, let's try with IPv6. */
    case 0:
      switch(dns_pton(AF_INET6, ip, &addr6)) {
        /* It's valid IPv6. */
        case 1:
          if (!dns_submit_a6ptr(dns_context, &addr6, dns_result_PTR_cb, (void *)data)) {
            xfree(data);
            raise_dns_error(dns_context);
          }
          break;
        /* Also an invalid IPv6 so raise an exception. */
        case 0:
          xfree(data);
          rb_raise(rb_eArgError, "invalid IP '%s' (neither IPv4 or IPv6)", ip);
          break;
      }
      break;
  }

  rb_hash_aset(rb_ivar_get(self, rb_intern("@queries")), query, Qtrue);
  return query;
}


VALUE Resolver_submit_MX(VALUE self, VALUE rb_domain)
{
  struct dns_ctx *dns_context;
  char *domain;
  VALUE query;
  struct resolver_query *data;

  Data_Get_Struct(self, struct dns_ctx, dns_context);
  domain = StringValueCStr(rb_domain);
  query = rb_obj_alloc(cQuery);

  data = ALLOC(struct resolver_query);
  data->resolver = self;
  data->query = query;

  if (!dns_submit_mx(dns_context, domain, 0, dns_result_MX_cb, (void *)data)) {
    xfree(data);
    raise_dns_error(dns_context);
  }

  rb_hash_aset(rb_ivar_get(self, rb_intern("@queries")), query, Qtrue);
  return query;
}


VALUE Resolver_submit_TXT(VALUE self, VALUE rb_domain)
{
  struct dns_ctx *dns_context;
  char *domain;
  VALUE query;
  struct resolver_query *data;

  Data_Get_Struct(self, struct dns_ctx, dns_context);
  domain = StringValueCStr(rb_domain);
  query = rb_obj_alloc(cQuery);

  data = ALLOC(struct resolver_query);
  data->resolver = self;
  data->query = query;

  if (!dns_submit_txt(dns_context, domain, DNS_C_IN, 0, dns_result_TXT_cb, (void *)data)) {
    xfree(data);
    raise_dns_error(dns_context);
  }

  rb_hash_aset(rb_ivar_get(self, rb_intern("@queries")), query, Qtrue);
  return query;
}


VALUE Resolver_submit_SRV(int argc, VALUE *argv, VALUE self)
{
  struct dns_ctx *dns_context;
  char *domain;
  char *service = NULL;
  char *protocol = NULL;
  VALUE query;
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
    xfree(data);
    raise_dns_error(dns_context);
  }
  
  rb_hash_aset(rb_ivar_get(self, rb_intern("@queries")), query, Qtrue);
  return query;
}


VALUE Resolver_submit_NAPTR(VALUE self, VALUE rb_domain)
{
  struct dns_ctx *dns_context;
  char *domain;
  VALUE query;
  struct resolver_query *data;

  Data_Get_Struct(self, struct dns_ctx, dns_context);
  domain = StringValueCStr(rb_domain);
  query = rb_obj_alloc(cQuery);

  data = ALLOC(struct resolver_query);
  data->resolver = self;
  data->query = query;

  if (!dns_submit_naptr(dns_context, domain, 0, dns_result_NAPTR_cb, (void *)data)) {
    xfree(data);
    raise_dns_error(dns_context);
  }

  rb_hash_aset(rb_ivar_get(self, rb_intern("@queries")), query, Qtrue);
  return query;
}


/* Attribute readers. */
VALUE RR_MX_domain(VALUE self)          { return rb_ivar_get(self, rb_intern("@domain")); }
VALUE RR_MX_priority(VALUE self)        { return rb_ivar_get(self, rb_intern("@priority")); }

VALUE RR_SRV_priority(VALUE self)       { return rb_ivar_get(self, rb_intern("@priority")); }
VALUE RR_SRV_weight(VALUE self)         { return rb_ivar_get(self, rb_intern("@weight")); }
VALUE RR_SRV_port(VALUE self)           { return rb_ivar_get(self, rb_intern("@port")); }
VALUE RR_SRV_domain(VALUE self)         { return rb_ivar_get(self, rb_intern("@domain")); }

VALUE RR_NAPTR_order(VALUE self)        { return rb_ivar_get(self, rb_intern("@order")); }
VALUE RR_NAPTR_preference(VALUE self)   { return rb_ivar_get(self, rb_intern("@preference")); }
VALUE RR_NAPTR_flags(VALUE self)        { return rb_ivar_get(self, rb_intern("@flags")); }
VALUE RR_NAPTR_service(VALUE self)      { return rb_ivar_get(self, rb_intern("@service")); }
VALUE RR_NAPTR_regexp(VALUE self)       { return rb_ivar_get(self, rb_intern("@regexp")); }
VALUE RR_NAPTR_replacement(VALUE self)  { return rb_ivar_get(self, rb_intern("@replacement")); }


void Init_em_udns_ext()
{
  mEm = rb_define_module("EventMachine");
  mEmDeferrable = rb_define_module_under(mEm, "Deferrable");
  mUdns = rb_define_module_under(mEm, "Udns");

  eUdnsError    = rb_define_class_under(mUdns, "UdnsError", rb_eStandardError);
  eUdnsTempFail = rb_define_class_under(mUdns, "UdnsTempFail", eUdnsError);
  eUdnsNoMem    = rb_define_class_under(mUdns, "UdnsNoMem", eUdnsError);
  eUdnsBadQuery = rb_define_class_under(mUdns, "UdnsBadQuery", eUdnsError);

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

  cQuery = rb_define_class_under(mUdns, "Query", rb_cObject);
  rb_include_module(cQuery, mEmDeferrable);

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
}
