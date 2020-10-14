/*
 * Copyright (c) 2009, Giampaolo Rodola'. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Useful resources:
// https://github.com/oblique/wificurse/blob/master/src/iw.c
// https://github.com/HewlettPackard/wireless-tools/blob/master/wireless_tools/iwconfig.c
// https://github.com/HewlettPackard/wireless-tools/blob/master/wireless_tools/iwlib.c
// https://github.com/vy/wapi/blob/master/src/wireless.c
// https://github.com/azbox-enigma2/pythonwifi/blob/master/pythonwifi/iwlibs.py

#include <Python.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/wireless.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

#include "../../_psutil_common.h"


#define SCAN_INTERVAL 100000  // 0.1 secs


// ====================================================================
// Wi-Fi card APIs
// ====================================================================

// --- utils

static int
ioctl_request(char *ifname, int request, struct iwreq *pwrq, int sock) {
    strncpy(pwrq->ifr_name, ifname, IFNAMSIZ);
    if (ioctl(sock, request, pwrq) == -1) {
        PyErr_SetFromOSErrnoWithSyscall("ioctl");
        return 1;
    }
    return 0;
}


static int
wifi_card_exists(char *ifname, int sock) {
    struct iwreq wrq;
    int ret;

    ret = ioctl_request(ifname, SIOCGIWNAME, &wrq, sock);
    if (ret == 0)
        return 1;
    else
        return 0;
}


static PyObject*
handle_ioctl_err(char *ifname, int sock, char *syscall) {
    if ((errno == ENOTSUP) || (errno == EINVAL)) {
        if (wifi_card_exists(ifname, sock) == 1) {
            PyErr_Clear();
            psutil_debug("%s failed; converting to None", syscall);
            Py_RETURN_NONE;
        }
        return NULL;
    }
    return NULL;
}


static char *
convert_macaddr(unsigned char *ptr) {
    static char buff[64];

    sprintf(buff, "%02X:%02X:%02X:%02X:%02X:%02X",
            (ptr[0] & 0xFF), (ptr[1] & 0xFF),
            (ptr[2] & 0xFF), (ptr[3] & 0xFF),
            (ptr[4] & 0xFF), (ptr[5] & 0xFF));
    return buff;
}


static double
freq2double(struct iw_freq freq) {
    // Expressed in Mb/sec.
    return ((double) freq.m) * pow(10, freq.e) / 1000 / 1000;
}


static char *
mode2str(int mode) {
   switch (mode) {
        case 0:
            return "auto";
        case 1:
            return "adhoc";
        case 2:
            return "managed";
        case 3:
            return "master";
        case 4:
            return "repeater";
        case 5:
            return "secondary";
        case 6:
            return "monitor";
        default:
            return "unknown";
    }
}


// --- APIs


/*
 * Given a Wi-Fi card name, return the ESSID (Wi-Fi name), if connected.
 * If not connected return an empty string.
 */
PyObject*
psutil_wifi_card_essid(PyObject* self, PyObject* args) {
    char *ifname;
    char *id;
    int sock;
    struct iwreq wrq;
    int size = IW_ESSID_MAX_SIZE + 1;
    PyObject *py_str;

    if (! PyArg_ParseTuple(args, "si", &ifname, &sock))
        return NULL;

    id = malloc(size);
    if (id == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    wrq.u.essid.pointer = (caddr_t) id;
    wrq.u.essid.length = size;
    wrq.u.essid.flags = 0;

    if (ioctl_request(ifname, SIOCGIWESSID, &wrq, sock) != 0) {
        free(id);
        return NULL;
    }

    py_str = Py_BuildValue("s", (char*)(wrq.u.essid.pointer));
    free(id);
    return py_str;
}


// The access point's MAC address.
PyObject*
psutil_wifi_card_bssid(PyObject* self, PyObject* args) {
    char *ifname;
    int sock;
    struct iwreq wrq;
    char *macaddr;

    if (! PyArg_ParseTuple(args, "si", &ifname, &sock))
        return NULL;
    if (ioctl_request(ifname, SIOCGIWAP, &wrq, sock) != 0)
        return NULL;
    macaddr = convert_macaddr((unsigned char*) &wrq.u.ap_addr.sa_data);
    if (strcmp(macaddr, "00:00:00:00:00:00") == 0)
        Py_RETURN_NONE;
    else
        return Py_BuildValue("s", macaddr);
}


/*
 * Wireless protocol (e.g. "IEEE 802.11"). If this fails, it means this
 * is not a Wi-Fi interface.
 */
PyObject*
psutil_wifi_card_proto(PyObject* self, PyObject* args) {
    char *ifname;
    int sock;
    struct iwreq wrq;

    if (! PyArg_ParseTuple(args, "si", &ifname, &sock))
        return NULL;
    if (ioctl_request(ifname, SIOCGIWNAME, &wrq, sock) != 0)
        return NULL;
    return Py_BuildValue("s", wrq.u.name);
}


PyObject*
psutil_wifi_card_mode(PyObject* self, PyObject* args) {
    char *ifname;
    int sock;
    struct iwreq wrq;

    if (! PyArg_ParseTuple(args, "si", &ifname, &sock))
        return NULL;
    if (ioctl_request(ifname, SIOCGIWMODE, &wrq, sock) != 0)
        return handle_ioctl_err(ifname, sock, "ioctl(SIOCGIWMODE)");
    return Py_BuildValue("s", mode2str(wrq.u.mode));
}


PyObject*
psutil_wifi_card_power_save(PyObject* self, PyObject* args) {
    char *ifname;
    int sock;
    struct iwreq wrq;

    if (! PyArg_ParseTuple(args, "si", &ifname, &sock))
        return NULL;
    if (ioctl_request(ifname, SIOCGIWPOWER, &wrq, sock) != 0)
        return handle_ioctl_err(ifname, sock, "ioctl(SIOCGIWPOWER)");
    if (wrq.u.power.disabled)
        Py_RETURN_FALSE;
    else
        Py_RETURN_TRUE;
}


PyObject*
psutil_wifi_card_frequency(PyObject* self, PyObject* args) {
    char *ifname;
    int sock;
    struct iwreq wrq;
    double freq;

    if (! PyArg_ParseTuple(args, "si", &ifname, &sock))
        return NULL;
    if (ioctl_request(ifname, SIOCGIWFREQ, &wrq, sock) != 0)
        return handle_ioctl_err(ifname, sock, "ioctl(SIOCGIWFREQ)");
    // expressed in Mb/sec
    freq = freq2double(wrq.u.freq);
    return Py_BuildValue("d", freq);
}


PyObject*
psutil_wifi_card_bitrate(PyObject* self, PyObject* args) {
    char *ifname;
    int sock;
    struct iwreq wrq;

    if (! PyArg_ParseTuple(args, "si", &ifname, &sock))
        return NULL;
    if (ioctl_request(ifname, SIOCGIWRATE, &wrq, sock) != 0)
        return handle_ioctl_err(ifname, sock, "ioctl(SIOCGIWRATE)");
    // Expressed in Mb/sec.
    return Py_BuildValue("d", (double)wrq.u.bitrate.value / 1000 / 1000);
}


PyObject*
psutil_wifi_card_txpower(PyObject* self, PyObject* args) {
    char *ifname;
    int sock;
    struct iwreq wrq;

    if (! PyArg_ParseTuple(args, "si", &ifname, &sock))
        return NULL;
    if (ioctl_request(ifname, SIOCGIWTXPOW, &wrq, sock) != 0)
        return handle_ioctl_err(ifname, sock, "ioctl(SIOCGIWTXPOW)");
    // Expressed in dbm.
    if (wrq.u.txpower.disabled)
        Py_RETURN_NONE;
    else
        return Py_BuildValue("i", wrq.u.txpower.value);
}


PyObject*
psutil_wifi_card_ranges(PyObject* self, PyObject* args) {
    char *ifname;
    int sock;
    struct iwreq wrq;
    char buffer[sizeof(struct iw_range) * 2];  // large enough
    struct iw_range *range;

    if (! PyArg_ParseTuple(args, "si", &ifname, &sock))
        return NULL;

    memset(buffer, 0, sizeof(buffer));

    wrq.u.data.pointer = (caddr_t) buffer;
    wrq.u.data.length = sizeof(buffer);
    wrq.u.data.flags = 0;

    if (ioctl_request(ifname, SIOCGIWRANGE, &wrq, sock) != 0)
        return NULL;

    range = (struct iw_range *) buffer;

    return Py_BuildValue(
        "Ii",
        range->max_qual.qual,              // link max quality (70)
        (int) range->max_qual.level - 256  // signal max quality (-110)
    );
}


/*
 * Get link quality and signal. These are the same values found in
 * /proc/net/wireless.
 */
PyObject*
psutil_wifi_card_stats(PyObject* self, PyObject* args) {
    char *ifname;
    int sock;
    struct iwreq wrq;
    struct iw_statistics stats;

    if (! PyArg_ParseTuple(args, "si", &ifname, &sock))
        return NULL;

    memset(&stats, 0, sizeof(stats));
    memset(&wrq, 0, sizeof(wrq));

    wrq.u.data.pointer = &stats;
    wrq.u.data.length = sizeof(stats);
    wrq.u.data.flags = 1;

    if (ioctl_request(ifname, SIOCGIWSTATS, &wrq, sock) != 0)
        return handle_ioctl_err(ifname, sock, "ioctl(SIOCGIWSTATS)");

    // signal: substract 256 in order to match /proc/net/wireless
    return Py_BuildValue(
        "IiIIIIII",
        stats.qual.qual,                    // link quality
        (int)stats.qual.level - 256,        // signal
        stats.discard.nwid,                 // Rx: wrong nwid/essid
        stats.discard.code,                 // Rx: unable to code/decode (WEP)
        stats.discard.fragment,             // Rx: can't perform MAC reassembly
        stats.discard.retries,              // Tx: max MAC retries num reached
        stats.discard.misc,                 // other cases
        stats.miss.beacon                   // missed beacons
    );
}


// ====================================================================
// Scan
// ====================================================================


#define SSID_MAX_LEN 32

typedef uint8_t u8;
typedef uint16_t u16;

struct wpa_scan_results {
    struct wpa_scan_res **res;
    size_t num;
};

struct wext_scan_data {
    struct wpa_scan_results res;
    u8 *ie;
    size_t ie_len;
    u8 ssid[SSID_MAX_LEN];
    size_t ssid_len;
    int maxrate;
};


static int
get_we_version(char *ifname, int sock) {
    struct iwreq wrq;
    char buffer[sizeof(struct iw_range) * 2];  // large enough
    struct iw_range *range;

    memset(buffer, 0, sizeof(buffer));
    wrq.u.data.pointer = (caddr_t) buffer;
    wrq.u.data.length = sizeof(buffer);
    wrq.u.data.flags = 0;

    if (ioctl_request(ifname, SIOCGIWRANGE, &wrq, sock) != 0)
        return -1;

    range = (struct iw_range *)buffer;
    return range->we_version_compiled;
}


static int
wext_19_iw_point(u16 cmd, int we_version) {
    return we_version > 18 &&
        (cmd == SIOCGIWESSID || cmd == SIOCGIWENCODE ||
         cmd == IWEVGENIE || cmd == IWEVCUSTOM);
}


static void
wext_get_scan_ssid(struct iw_event *iwe,
                   struct wext_scan_data *res,
                   char *custom,
                   char *end)
{
    int ssid_len = iwe->u.essid.length;
    if (custom + ssid_len > end)
        return;
    if (iwe->u.essid.flags && ssid_len > 0 && ssid_len <= IW_ESSID_MAX_SIZE) {
        memcpy(res->ssid, custom, ssid_len);
        res->ssid_len = ssid_len;
    }
}


static PyObject*
parse_scan(char *res_buf, int len, char *ifname, int skfd) {
    char *pos, *end, *custom;
    char *bssid;
    struct iw_event iwe_buf, *iwe = &iwe_buf;
    struct wext_scan_data data;
    struct wpa_scan_results *res;
    int we_version;

    we_version = get_we_version(ifname, skfd);
    if (we_version == -1)
        return NULL;

    res = malloc(sizeof(*res));
    if (res == NULL) {
        free(res_buf);
        return NULL;
    }

    pos = (char *) res_buf;
    end = (char *) res_buf + len;
    memset(&data, 0, sizeof(data));

    while (pos + IW_EV_LCP_LEN <= end) {
        memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
        if (iwe->len <= IW_EV_LCP_LEN)
            break;

        custom = pos + IW_EV_POINT_LEN;
        if (wext_19_iw_point(iwe->cmd, we_version)) {
            char *dpos = (char *) &iwe_buf.u.data.length;
            int dlen = dpos - (char *) &iwe_buf;
            memcpy(dpos, pos + IW_EV_LCP_LEN,
                  sizeof(struct iw_event) - dlen);
        }
        else {
            memcpy(&iwe_buf, pos, sizeof(struct iw_event));
            custom += IW_EV_POINT_OFF;
        }

        //
        if (iwe->cmd == SIOCGIWAP) {
            bssid = convert_macaddr((unsigned char*) &iwe->u.ap_addr.sa_data);
            printf("bssid: %s\n", bssid);
        }
        else if (iwe->cmd == SIOCGIWESSID) {
            wext_get_scan_ssid(iwe, &data, custom, end);
            printf("ssid: %s\n", data.ssid);
        }

        // go to next
        pos += iwe->len;
    }

    return Py_BuildValue("i", 33);
}


PyObject*
psutil_wifi_scan(PyObject* self, PyObject* args) {
    int skfd = -1;
    int ret;
    char *ifname;
    struct iwreq wrq;
    char *buffer = NULL;
    char *newbuf;
    int buflen = IW_SCAN_MAX_DATA;
    PyObject *py_ret;

    if (! PyArg_ParseTuple(args, "s", &ifname))
        return NULL;

    // setup iwreq struct
    wrq.u.data.pointer = NULL;
    wrq.u.data.flags = 0;
    wrq.u.data.length = 0;
    strncpy(wrq.ifr_name, ifname, IFNAMSIZ);

    // create socket
    skfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (skfd == -1) {
        PyErr_SetFromOSErrnoWithSyscall("socket()");
        goto error;
    }

    // set scan
    ret = ioctl(skfd, SIOCSIWSCAN, &wrq);
    if (ret == -1) {
        PyErr_SetFromOSErrnoWithSyscall("ioctl(SIOCSIWSCAN)");
        goto error;
    }

    // get scan
    while (1) {
        // (Re)allocate the buffer - realloc(NULL, len) == malloc(len).
        newbuf = realloc(buffer, buflen);
        if (newbuf == NULL) {
            // man says: if realloc() fails the original block is left
            // untouched.
            if (buffer) {
                free(buffer);
                buffer = NULL;
            }
            PyErr_NoMemory();
            goto error;
        }
        buffer = newbuf;

        wrq.u.data.pointer = buffer;
        wrq.u.data.flags = 0;
        wrq.u.data.length = buflen;

        ret = ioctl(skfd, SIOCGIWSCAN, &wrq);
        if (ret < 0) {
            if (errno == E2BIG) {
                // Some driver may return very large scan results, either
                // because there are many cells, or because they have many
                // large elements in cells (like IWEVCUSTOM). Most will
                // only need the regular sized buffer. We now use a dynamic
                // allocation of the buffer to satisfy everybody. Of course,
                // as we don't know in advance the size of the array, we try
                // various increasing sizes. Jean II

                // Check if the driver gave us any hints.
                psutil_debug("ioctl(SIOCGIWSCAN) -> E2BIG");
                if (wrq.u.data.length > buflen)
                    buflen = wrq.u.data.length;
                else
                    buflen *= 2;
                usleep(SCAN_INTERVAL);
                continue;
            }
            else if (errno == EAGAIN) {
                psutil_debug("ioctl(SIOCGIWSCAN) -> EAGAIN");
                usleep(SCAN_INTERVAL);
                continue;
            }
            else {
                PyErr_SetFromOSErrnoWithSyscall("ioctl(SIOCGIWSCAN)");
                goto error;
            }
        }
        break;
    }

    py_ret = parse_scan(buffer, wrq.u.data.length, ifname, skfd);
    close(skfd);
    free(buffer);
    return py_ret;

error:
    if (skfd != -1)
        close(skfd);
    if (buffer != NULL)
        free(buffer);
    return NULL;
}