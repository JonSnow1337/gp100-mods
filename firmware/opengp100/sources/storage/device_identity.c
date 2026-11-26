#include "device_identity.h"

#include <stddef.h>
#include <string.h>

/* Factory table at SDRAM 0x800f864c. Only product indices 0..49 are read. */
static const uint8_t k_substitution_indices[50] = {
    0x66u, 0x32u, 0x76u, 0x30u, 0x3bu, 0x36u, 0x1au, 0x75u,
    0x20u, 0x19u, 0x7fu, 0x63u, 0x1cu, 0x6au, 0x11u, 0x22u,
    0x4fu, 0x0cu, 0x77u, 0x02u, 0x0fu, 0x56u, 0x20u, 0x6du,
    0x7fu, 0x36u, 0x5cu, 0x52u, 0x3cu, 0x6cu, 0x50u, 0x2eu,
    0x15u, 0x13u, 0x14u, 0x57u, 0x01u, 0x5au, 0x29u, 0x42u,
    0x01u, 0x05u, 0x16u, 0x49u, 0x2fu, 0x43u, 0x36u, 0x75u,
    0x2cu, 0x68u
};

bool gp100_decode_device_identity(
    const uint8_t record[GP100_DEVICE_RECORD_SIZE],
    uint8_t identity[GP100_DEVICE_ID_SIZE])
{
    size_t byte_index;

    if (record == NULL || identity == NULL) {
        return false;
    }
    memcpy(identity, record, GP100_DEVICE_ID_SIZE);
    for (byte_index = 0u; byte_index < GP100_DEVICE_ID_SIZE;
         ++byte_index) {
        size_t round = GP100_DEVICE_ID_SIZE;
        while (round != 0u) {
            uint8_t table_index;
            --round;
            table_index = k_substitution_indices[round * byte_index];
            identity[byte_index] ^=
                (uint8_t)~record[GP100_DEVICE_ID_SIZE + table_index];
        }
        identity[byte_index] = (uint8_t)~identity[byte_index];
    }
    return true;
}

bool gp100_device_identity_matches(
    const uint8_t record[GP100_DEVICE_RECORD_SIZE],
    const uint8_t ocotp_identity[GP100_DEVICE_ID_SIZE])
{
    uint8_t decoded[GP100_DEVICE_ID_SIZE];

    return ocotp_identity != NULL &&
           gp100_decode_device_identity(record, decoded) &&
           memcmp(decoded, ocotp_identity, sizeof(decoded)) == 0;
}
