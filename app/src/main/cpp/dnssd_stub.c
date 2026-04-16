// dnssd stub for FirePlay. UxPlay's lib expects libdns_sd. On Android we
// publish via JmDNS (Kotlin); but UxPlay's GET /info handler still calls
// dnssd_get_*_txt() to embed our TXT records in the response plist iPhone
// uses to negotiate. Iphone gives up if these are empty, so build them
// in proper Bonjour DNS-TXT binary format here.
//
// TXT format: each entry is one byte of length followed by `key=value`.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../../../lib-uxplay/dnssd.h"

#define TXT_BUF_MAX 1024

struct dnssd_s {
    char name[256];
    char hw_addr[6];
    int hw_addr_len;
    uint64_t features;
    uint32_t features1;
    uint32_t features2;
    char *pk;
    unsigned char raop_txt[TXT_BUF_MAX];
    int raop_txt_len;
    unsigned char airplay_txt[TXT_BUF_MAX];
    int airplay_txt_len;
};

static int txt_append(unsigned char *buf, int *len, const char *key, const char *val) {
    size_t klen = strlen(key), vlen = strlen(val);
    size_t entry = klen + 1 + vlen;
    if (entry > 255) return -1;
    if (*len + 1 + (int)entry > TXT_BUF_MAX) return -1;
    buf[(*len)++] = (unsigned char)entry;
    memcpy(buf + *len, key, klen); *len += (int)klen;
    buf[(*len)++] = '=';
    memcpy(buf + *len, val, vlen); *len += (int)vlen;
    return 0;
}

static void hwaddr_to_airplay(const char *hw, int hwlen, char *out) {
    /* AirPlay deviceid: aa:bb:cc:dd:ee:ff */
    int o = 0;
    for (int i = 0; i < hwlen; i++) {
        if (i) out[o++] = ':';
        sprintf(out + o, "%02X", (unsigned char)hw[i]); o += 2;
    }
    out[o] = 0;
}

static void hwaddr_to_raop(const char *hw, int hwlen, char *out) {
    /* RAOP service prefix: AABBCCDDEEFF */
    int o = 0;
    for (int i = 0; i < hwlen; i++) {
        sprintf(out + o, "%02X", (unsigned char)hw[i]); o += 2;
    }
    out[o] = 0;
}

static void rebuild_txt_records(dnssd_t *d) {
    char features[24];
    snprintf(features, sizeof(features), "0x%X,0x%X", d->features1, d->features2);
    char device_id[24];
    hwaddr_to_airplay(d->hw_addr, d->hw_addr_len, device_id);

    /* _airplay._tcp TXT */
    d->airplay_txt_len = 0;
    txt_append(d->airplay_txt, &d->airplay_txt_len, "deviceid",  device_id);
    txt_append(d->airplay_txt, &d->airplay_txt_len, "features",  features);
    txt_append(d->airplay_txt, &d->airplay_txt_len, "flags",     "0x4");
    txt_append(d->airplay_txt, &d->airplay_txt_len, "model",     "AppleTV3,2");
    txt_append(d->airplay_txt, &d->airplay_txt_len, "pk",        d->pk ? d->pk : "");
    txt_append(d->airplay_txt, &d->airplay_txt_len, "pi",        "2e388006-13ba-4041-9a67-25dd4a43d536");
    txt_append(d->airplay_txt, &d->airplay_txt_len, "pw",        "false");
    txt_append(d->airplay_txt, &d->airplay_txt_len, "srcvers",   "220.68");
    txt_append(d->airplay_txt, &d->airplay_txt_len, "vv",        "2");

    /* _raop._tcp TXT */
    d->raop_txt_len = 0;
    txt_append(d->raop_txt, &d->raop_txt_len, "ch",      "2");
    txt_append(d->raop_txt, &d->raop_txt_len, "cn",      "0,1,2,3");
    txt_append(d->raop_txt, &d->raop_txt_len, "da",      "true");
    txt_append(d->raop_txt, &d->raop_txt_len, "et",      "0,3,5");
    txt_append(d->raop_txt, &d->raop_txt_len, "vv",      "2");
    txt_append(d->raop_txt, &d->raop_txt_len, "ft",      features);
    txt_append(d->raop_txt, &d->raop_txt_len, "am",      "AppleTV3,2");
    txt_append(d->raop_txt, &d->raop_txt_len, "md",      "0,1,2");
    txt_append(d->raop_txt, &d->raop_txt_len, "rhd",     "5.6.0.0");
    txt_append(d->raop_txt, &d->raop_txt_len, "pw",      "false");
    txt_append(d->raop_txt, &d->raop_txt_len, "sf",      "0x4");
    txt_append(d->raop_txt, &d->raop_txt_len, "sr",      "44100");
    txt_append(d->raop_txt, &d->raop_txt_len, "ss",      "16");
    txt_append(d->raop_txt, &d->raop_txt_len, "sv",      "false");
    txt_append(d->raop_txt, &d->raop_txt_len, "tp",      "UDP");
    txt_append(d->raop_txt, &d->raop_txt_len, "txtvers", "1");
    txt_append(d->raop_txt, &d->raop_txt_len, "vs",      "220.68");
    txt_append(d->raop_txt, &d->raop_txt_len, "vn",      "65537");
    txt_append(d->raop_txt, &d->raop_txt_len, "pk",      d->pk ? d->pk : "");
}

dnssd_t *dnssd_init(const char *name, int name_len, const char *hw_addr, int hw_addr_len, int *error, unsigned char pin_pw) {
    (void)pin_pw;
    dnssd_t *d = (dnssd_t*)calloc(1, sizeof(*d));
    if (!d) { if (error) *error = -1; return NULL; }
    if (name_len > 0 && name_len < (int)sizeof(d->name)) memcpy(d->name, name, name_len);
    if (hw_addr_len > 0 && hw_addr_len <= (int)sizeof(d->hw_addr)) {
        memcpy(d->hw_addr, hw_addr, hw_addr_len);
        d->hw_addr_len = hw_addr_len;
    }
    /* Default AirPlay 2 features bitmap matching UxPlay reference. */
    d->features  = 0x527FFEE6ULL;
    d->features1 = 0x527FFEE6;
    d->features2 = 0x0;
    rebuild_txt_records(d);
    if (error) *error = 0;
    return d;
}
int  dnssd_register_raop(dnssd_t *d, unsigned short port)     { (void)d; (void)port; return 0; }
int  dnssd_register_airplay(dnssd_t *d, unsigned short port)  { (void)d; (void)port; return 0; }
void dnssd_unregister_raop(dnssd_t *d)                         { (void)d; }
void dnssd_unregister_airplay(dnssd_t *d)                      { (void)d; }
const char *dnssd_get_raop_txt(dnssd_t *d, int *length)        { if (length) *length = d->raop_txt_len; return (const char*)d->raop_txt; }
const char *dnssd_get_airplay_txt(dnssd_t *d, int *length)     { if (length) *length = d->airplay_txt_len; return (const char*)d->airplay_txt; }
const char *dnssd_get_name(dnssd_t *d, int *length)            { if (length) *length = (int)strlen(d->name); return d->name; }
const char *dnssd_get_hw_addr(dnssd_t *d, int *length)         { if (length) *length = d->hw_addr_len; return d->hw_addr; }
void  dnssd_set_airplay_features(dnssd_t *d, int bit, int val) {
    if (val) d->features |=  ((uint64_t)1 << bit);
    else     d->features &= ~((uint64_t)1 << bit);
    d->features1 = (uint32_t)(d->features & 0xFFFFFFFF);
    d->features2 = (uint32_t)(d->features >> 32);
    rebuild_txt_records(d);
}
uint64_t dnssd_get_airplay_features(dnssd_t *d)                { return d->features; }
void dnssd_set_pk(dnssd_t *d, char *pk_str)                    {
    free(d->pk);
    d->pk = pk_str ? strdup(pk_str) : NULL;
    rebuild_txt_records(d);
}
void dnssd_destroy(dnssd_t *d)                                 {
    if (!d) return;
    free(d->pk);
    free(d);
}
