// dnssd stub for FirePlay. UxPlay's lib expects a libdns_sd-style API
// to publish AirPlay/RAOP services. On Android we publish via JmDNS in
// Kotlin instead, so all calls here are no-ops that succeed and leak no
// state. Only the TXT-record build path in dnssd.c still matters and we
// retain it by exposing dnssd_get_*_txt() from a small dnssd_t struct.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../../../lib-uxplay/dnssd.h"

struct dnssd_s {
    char name[256];
    char hw_addr[6];
    int hw_addr_len;
    uint64_t features;
    char *pk;
    char *raop_txt; int raop_txt_len;
    char *airplay_txt; int airplay_txt_len;
};

dnssd_t *dnssd_init(const char *name, int name_len, const char *hw_addr, int hw_addr_len, int *error, unsigned char pin_pw) {
    (void)pin_pw;
    dnssd_t *d = (dnssd_t*)calloc(1, sizeof(*d));
    if (!d) { if (error) *error = -1; return NULL; }
    if (name_len > 0 && name_len < (int)sizeof(d->name)) memcpy(d->name, name, name_len);
    if (hw_addr_len > 0 && hw_addr_len <= (int)sizeof(d->hw_addr)) {
        memcpy(d->hw_addr, hw_addr, hw_addr_len);
        d->hw_addr_len = hw_addr_len;
    }
    if (error) *error = 0;
    return d;
}
int  dnssd_register_raop(dnssd_t *d, unsigned short port)     { (void)d; (void)port; return 0; }
int  dnssd_register_airplay(dnssd_t *d, unsigned short port)  { (void)d; (void)port; return 0; }
void dnssd_unregister_raop(dnssd_t *d)                         { (void)d; }
void dnssd_unregister_airplay(dnssd_t *d)                      { (void)d; }
const char *dnssd_get_raop_txt(dnssd_t *d, int *length)        { if (length) *length = d->raop_txt_len; return d->raop_txt; }
const char *dnssd_get_airplay_txt(dnssd_t *d, int *length)     { if (length) *length = d->airplay_txt_len; return d->airplay_txt; }
const char *dnssd_get_name(dnssd_t *d, int *length)            { if (length) *length = (int)strlen(d->name); return d->name; }
const char *dnssd_get_hw_addr(dnssd_t *d, int *length)         { if (length) *length = d->hw_addr_len; return d->hw_addr; }
void  dnssd_set_airplay_features(dnssd_t *d, int bit, int val) {
    if (val) d->features |=  ((uint64_t)1 << bit);
    else     d->features &= ~((uint64_t)1 << bit);
}
uint64_t dnssd_get_airplay_features(dnssd_t *d)                { return d->features; }
void dnssd_set_pk(dnssd_t *d, char *pk_str)                    {
    free(d->pk);
    d->pk = pk_str ? strdup(pk_str) : NULL;
}
void dnssd_destroy(dnssd_t *d)                                 {
    if (!d) return;
    free(d->pk);
    free(d->raop_txt);
    free(d->airplay_txt);
    free(d);
}
