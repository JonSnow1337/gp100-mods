#include "protocol/midi/sysex/vendor_sysex_user_amp.h"

#include <stdio.h>
#include <string.h>

static int test_model_layout(const char *path)
{
    static const uint16_t crc16_values[4] = {
        UINT16_C(0x666e), UINT16_C(0xcd78), UINT16_C(0x0465),
        UINT16_C(0xf461)};
    uint8_t blob[GP100_SYSEX_USER_AMP_BLOB_BYTES];
    gp100_user_amp_model_layout_t layout;
    FILE *file = fopen(path, "rb");
    size_t slot;
    uint16_t crc16;

    if (file == NULL)
    {
        return 1;
    }
    for (slot = 0u; slot < GP100_SYSEX_USER_AMP_SLOT_COUNT; ++slot)
    {
        long offset =
            (long)(GP100_USER_AMP_BLOB_FLASH_BASE + slot * sizeof(blob));

        if (fseek(file, offset, SEEK_SET) != 0 ||
            fread(blob, 1u, sizeof(blob), file) != sizeof(blob))
        {
            fclose(file);
            return 2;
        }
        if (slot == 4u)
        {
            if (gp100_user_amp_model_layout(blob, sizeof(blob), &layout))
            {
                fclose(file);
                return 3;
            }
            continue;
        }
        if (!gp100_user_amp_model_layout(blob, sizeof(blob), &layout) ||
            !gp100_user_amp_model_crc16(
                blob, sizeof(blob), &crc16) ||
            layout.declared_bytes != UINT32_C(0x1288) ||
            layout.crc16 != crc16_values[slot] ||
            crc16 != crc16_values[slot] ||
            layout.payload_bytes != UINT32_C(0x1200) ||
            layout.input_biquad[0] != 1.0 || layout.input_biquad[1] != 0.0 ||
            layout.input_biquad[2] != 0.0 || layout.input_biquad[3] != 0.0 ||
            layout.input_biquad[4] != 0.0 || layout.positive_limit <= 0.0f ||
            layout.negative_limit <= 0.0f || layout.positive_rate <= 0.0f ||
            layout.negative_rate <= 0.0f ||
            layout.first_bank_offset_words != 0u ||
            layout.first_bank_value_count != UINT32_C(0x80) ||
            layout.second_bank_offset_words != UINT32_C(0x80) ||
            layout.second_bank_value_count != UINT32_C(0x400))
        {
            fclose(file);
            return 4;
        }
        if (slot == 0u)
        {
            blob[GP100_USER_AMP_MODEL_HEADER_BYTES] ^= 1u;
            if (gp100_user_amp_model_layout(blob, sizeof(blob), &layout))
            {
                fclose(file);
                return 5;
            }
            if (!gp100_user_amp_model_update_crc16(blob, sizeof(blob)) ||
                !gp100_user_amp_model_layout(blob, sizeof(blob), &layout) ||
                layout.crc16 == crc16_values[slot])
            {
                fclose(file);
                return 6;
            }
            blob[GP100_USER_AMP_MODEL_HEADER_BYTES] ^= 1u;
            if (!gp100_user_amp_model_update_crc16(blob, sizeof(blob)) ||
                !gp100_user_amp_model_layout(blob, sizeof(blob), &layout) ||
                layout.crc16 != crc16_values[slot])
            {
                fclose(file);
                return 7;
            }
            blob[0x04u] = 0x87u;
            if (gp100_user_amp_model_layout(blob, sizeof(blob), &layout))
            {
                fclose(file);
                return 8;
            }
            blob[0x04u] = 0x88u;
            blob[0x12u] = 1u;
            if (gp100_user_amp_model_layout(blob, sizeof(blob), &layout))
            {
                fclose(file);
                return 9;
            }
            blob[0x12u] = 0u;
            blob[0x84u] = 1u;
            if (gp100_user_amp_model_layout(blob, sizeof(blob), &layout))
            {
                fclose(file);
                return 10;
            }
            blob[0x84u] = 0u;
            blob[0x14u] = 1u;
            if (gp100_user_amp_model_layout(blob, sizeof(blob), &layout))
            {
                fclose(file);
                return 11;
            }
        }
    }
    fclose(file);
    return gp100_user_amp_model_update_crc16(NULL, 0u) ? 12 : 0;
}

static int test_descriptors(void)
{
    uint8_t request_payload[] = {3u};
    gp100_sysex_view_t request = {
        0u, GP100_SYSEX_REQUEST_CLASS,
        GP100_SYSEX_USER_AMP_DESCRIPTOR_READ, request_payload,
        sizeof(request_payload)};
    gp100_sysex_user_amp_record_t source = {true, {0}};
    gp100_sysex_user_amp_record_t decoded;
    gp100_sysex_view_t response_view;
    uint8_t response[GP100_SYSEX_FACTORY_MAX_MESSAGE_BYTES];
    uint8_t user_index = 0xffu;
    size_t response_length;

    memcpy(source.metadata, "Reference amp", 13u);
    if (gp100_sysex_user_amp_build_descriptor(
            &request, &source, response, sizeof(response), &response_length) !=
            GP100_SYSEX_USER_AMP_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &response_view) ||
        response_view.address != GP100_SYSEX_USER_AMP_DESCRIPTOR_READ ||
        !gp100_sysex_user_amp_decode_descriptor(
            response_view.payload, response_view.payload_length, &user_index,
            &decoded) ||
        user_index != 3u || !decoded.present ||
        memcmp(decoded.metadata, source.metadata, sizeof(decoded.metadata)) !=
            0)
    {
        return 1;
    }

    source.present = false;
    memset(source.metadata, 0xa5, sizeof(source.metadata));
    if (gp100_sysex_user_amp_build_descriptor(
            &request, &source, response, sizeof(response), &response_length) !=
            GP100_SYSEX_USER_AMP_RESPONSE ||
        !gp100_sysex_parse(response, response_length, &response_view) ||
        !gp100_sysex_user_amp_decode_descriptor(
            response_view.payload, response_view.payload_length, &user_index,
            &decoded) ||
        decoded.present)
    {
        return 2;
    }
    for (user_index = 0u; user_index < sizeof(decoded.metadata); ++user_index)
    {
        if (decoded.metadata[user_index] != 0u)
        {
            return 3;
        }
    }
    return 0;
}

static int test_reset(void)
{
    uint8_t payload[] = {0u, 4u};
    gp100_sysex_view_t command = {0u, GP100_SYSEX_RESPONSE_CLASS,
                                       GP100_SYSEX_USER_AMP_RESET, payload,
                                       sizeof(payload)};
    gp100_sysex_user_amp_reset_action_t action;
    gp100_sysex_user_amp_record_t record;

    memset(&record, 0xa5, sizeof(record));
    if (gp100_sysex_user_amp_decode_reset(&command, &action) !=
            GP100_SYSEX_USER_AMP_ACTION ||
        action.user_index != 4u ||
        !gp100_sysex_user_amp_apply_reset(&record, &action) ||
        record.present || memcmp(record.metadata, "SnapTone5", 10u) != 0)
    {
        return 1;
    }
    for (size_t index = 10u; index < sizeof(record.metadata); ++index)
    {
        if (record.metadata[index] != 0u)
            return 2;
    }
    payload[1] = 5u;
    if (gp100_sysex_user_amp_decode_reset(&command, &action) !=
        GP100_SYSEX_USER_AMP_INVALID)
    {
        return 3;
    }
    return 0;
}

static int test_full_upload(void)
{
    uint8_t package[GP100_SYSEX_USER_AMP_PACKAGE_BYTES];
    uint8_t completed[GP100_SYSEX_USER_AMP_PACKAGE_BYTES];
    uint8_t workspace[GP100_SYSEX_USER_AMP_UPLOAD_ENCODED_BYTES];
    uint8_t payload[GP100_SYSEX_USER_AMP_UPLOAD_PAYLOAD_BYTES];
    uint8_t response[32];
    gp100_sysex_user_amp_upload_t upload;
    gp100_sysex_view_t command = {0u, GP100_SYSEX_RESPONSE_CLASS,
                                       GP100_SYSEX_USER_AMP_UPLOAD,
                                       payload, sizeof(payload)};
    size_t response_length;
    size_t chunk;
    size_t index;
    uint8_t completed_index = 0xffu;

    for (index = 0u; index < sizeof(package); ++index)
    {
        package[index] = (uint8_t)(index * 29u + 11u);
    }
    if (!gp100_sysex_user_amp_upload_init(&upload, workspace,
                                               sizeof(workspace)))
    {
        return 1;
    }
    for (chunk = 0u; chunk < GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_COUNT;
         ++chunk)
    {
        if (!gp100_sysex_user_amp_build_upload_payload(
                4u, (uint16_t)chunk, package, payload) ||
            gp100_sysex_user_amp_handle_upload(
                &upload, &command, completed, &completed_index, response,
                sizeof(response), &response_length) !=
            (chunk + 1u == GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_COUNT
                 ? GP100_SYSEX_USER_AMP_DONE
                 : GP100_SYSEX_USER_AMP_RESPONSE))
        {
            return 2;
        }
    }
    if (completed_index != 4u ||
        memcmp(completed, package, sizeof(package)) != 0)
    {
        return 3;
    }
    for (index = 15u; index <= 30u; ++index)
    {
        if (payload[index] !=
            GP100_SYSEX_USER_AMP_UPLOAD_PADDING_NIBBLE)
        {
            return 4;
        }
    }
    if (gp100_sysex_user_amp_build_upload_payload(
            GP100_SYSEX_USER_AMP_SLOT_COUNT, 0u, package, payload) ||
        gp100_sysex_user_amp_build_upload_payload(
            0u, GP100_SYSEX_USER_AMP_UPLOAD_CHUNK_COUNT,
            package, payload) ||
        gp100_sysex_user_amp_build_upload_payload(
            0u, 0u, NULL, payload))
    {
        return 5;
    }
    return 0;
}

static int test_name_update(void)
{
    uint8_t payload[4u + GP100_SYSEX_USER_AMP_NAME_BYTES] = {
        0};
    gp100_sysex_view_t command = {
        0u, GP100_SYSEX_RESPONSE_CLASS,
        GP100_SYSEX_USER_AMP_NAME_UPDATE, payload, sizeof(payload)};
    gp100_sysex_user_amp_name_action_t action;
    gp100_sysex_user_amp_record_t record;
    size_t index;

    memset(&record, 0xa5, sizeof(record));
    record.present = true;
    gp100_sysex_encode_u16(2u, payload);
    memcpy(payload + 4u, "Updated model", 14u);
    if (gp100_sysex_user_amp_decode_name_update(&command, &action) !=
            GP100_SYSEX_USER_AMP_ACTION ||
        action.user_index != 2u ||
        memcmp(action.name, payload + 4u, sizeof(action.name)) != 0 ||
        !gp100_sysex_user_amp_apply_name(&record, &action) ||
        !record.present ||
        memcmp(record.metadata, payload + 4u,
               GP100_SYSEX_USER_AMP_NAME_BYTES) != 0)
    {
        return 1;
    }
    for (index = GP100_SYSEX_USER_AMP_NAME_BYTES;
         index < sizeof(record.metadata); ++index)
    {
        if (record.metadata[index] != 0u)
        {
            return 2;
        }
    }
    gp100_sysex_encode_u16(5u, payload);
    if (gp100_sysex_user_amp_decode_name_update(&command, &action) !=
        GP100_SYSEX_USER_AMP_INVALID)
    {
        return 3;
    }
    return 0;
}

static int test_storage_descriptor(void)
{
    uint8_t metadata[GP100_SYSEX_USER_AMP_METADATA_BYTES];
    uint8_t record[GP100_USER_AMP_DESCRIPTOR_RECORD_BYTES];
    uint32_t descriptor_offset;
    uint32_t blob_offset;
    gp100_sysex_user_amp_record_t decoded;
    size_t index;

    for (index = 0u; index < sizeof(metadata); ++index)
        metadata[index] = (uint8_t)(index * 7u + 3u);
    if (!gp100_user_amp_flash_offsets(4u, &descriptor_offset,
                                           &blob_offset) ||
        descriptor_offset != UINT32_C(0x006c00e0) ||
        blob_offset != UINT32_C(0x006c9000) ||
        !gp100_user_amp_build_descriptor_record(4u, true, metadata,
                                                     record) ||
        !gp100_user_amp_validate_descriptor_record(4u, record) ||
        !gp100_user_amp_decode_descriptor_record(4u, record, &decoded) ||
        !decoded.present ||
        memcmp(decoded.metadata, metadata, sizeof(metadata)) != 0 ||
        memcmp(record + 0x0cu, metadata, sizeof(metadata)) != 0 ||
        gp100_user_amp_flash_offsets(5u, &descriptor_offset, &blob_offset))
    {
        return 1;
    }
    record[0x10] ^= 1u;
    if (gp100_user_amp_validate_descriptor_record(4u, record))
        return 2;
    return 0;
}

int main(int argc, char **argv)
{
    int result = 0;

    if (argc != 2 || (result = test_model_layout(argv[1])) != 0)
    {
        fprintf(stderr, "user amp model layout test failed: %d\n", result);
        return 5 + result;
    }
    result = test_descriptors();
    if (result != 0)
    {
        fprintf(stderr, "user amp descriptor test failed: %d\n", result);
        return 10 + result;
    }
    result = test_reset();
    if (result != 0)
    {
        fprintf(stderr, "user amp reset test failed: %d\n", result);
        return 20 + result;
    }
    result = test_full_upload();
    if (result != 0)
    {
        fprintf(stderr, "user amp upload test failed: %d\n", result);
        return 30 + result;
    }
    result = test_name_update();
    if (result != 0)
    {
        fprintf(stderr, "user amp name test failed: %d\n", result);
        return 40 + result;
    }
    result = test_storage_descriptor();
    if (result != 0)
    {
        fprintf(stderr, "user amp storage test failed: %d\n", result);
        return 50 + result;
    }
    return 0;
}
