#include <string.h>

#define ADDRESS_LENGTH 5	// valid range: 3-5
#define PAYLOAD_LENGTH 10	// valid range: 1-32

/*
 * ShockBurst packet format (length in bytes in parenthesis):
 *
 * +--------------+---------------+-------------------------------+-----------+
 * | Preamble (1) | Address (3-5) | Payload (1-32)                | CRC (1-2) |
 * +--------------+---------------+-------------------------------+-----------+
 *
 * CRC is calculated over the "Address" and "Payload" fields, and it is 2 bytes
 * in case of ANT.
 */
typedef struct {
	uint64_t address;
	uint8_t payload[PAYLOAD_LENGTH];
	uint16_t crc;
} ShockBurstPacket;

#define FOREACH_STATE(STATE) \
		STATE(LINK)   \
		STATE(AUTHENTICATION)  \
		STATE(TRANSPORT)   \
		STATE(BUSY)  \
		STATE(RESERVED) \

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

enum DEVICE_STATE {
    FOREACH_STATE(GENERATE_ENUM)
};

static const char *STATE_STRING[] = {
    FOREACH_STATE(GENERATE_STRING)
};

uint16_t crc16(uint8_t* data, size_t length)
{
	int i;
	uint16_t crc = 0xffff;

	while (length--) {
		crc ^= *(unsigned char *)data++ << 8;
		for (i = 0; i < 8; ++i)
			crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
	}

	return crc & 0xffff;
}

bool is_sbp(uint8_t* bytes)
{
	uint16_t packet_crc, calced_crc;

	calced_crc = crc16(bytes, ADDRESS_LENGTH + PAYLOAD_LENGTH);
	packet_crc = bytes[ADDRESS_LENGTH + PAYLOAD_LENGTH] << 8 |
		bytes[ADDRESS_LENGTH + PAYLOAD_LENGTH + 1];

	return calced_crc == packet_crc;
} 

bool sbp_from_bytes(uint8_t* bytes, ShockBurstPacket* packet)
{
	int i;
	uint16_t packet_crc, calced_crc;

	calced_crc = crc16(bytes, ADDRESS_LENGTH + PAYLOAD_LENGTH);
	packet_crc = bytes[ADDRESS_LENGTH + PAYLOAD_LENGTH] << 8 |
		bytes[ADDRESS_LENGTH + PAYLOAD_LENGTH + 1];

	if (calced_crc == packet_crc) {
		packet->crc = packet_crc;

		packet->address = 0;
		for (i = 0; i < ADDRESS_LENGTH; ++i) {
			packet->address |=
				((uint64_t)bytes[i]) << (ADDRESS_LENGTH - 1 - i) * 8;
		}

		memcpy(packet->payload, bytes + ADDRESS_LENGTH, PAYLOAD_LENGTH);

		return true;
	}

	return false;
}

/*
bool ant_from_sbp(ShockBurstPacket* packet, char* packet_json, size_t len)
{
	switch(packet->payload[2]) {
		case 0x43: {	// ANT-FS Client Beacon
				uint8_t status1 = packet->payload[3];
				uint8_t status2 = packet->payload[4];
				float period;
				
				switch(status1 & 7) {
					case 0: period = 0.5; break;
					case 1: period = 1; break;
					case 2: period = 2; break;
					case 3: period = 4; break;
					case 4: period = 8; break;
					case 7: period = -1; break;	// match established (ANT-FS broadcast)
					defaul: period = 0; break;	// reserved
				}

				switch(status2 & 0xf) {
					case 0: break;
					case 1: break;
					case 2: break;
					case 3: break;
					defaut: break;
				}

				snprintf(packet_json, len,
						"{\"type\": \"client beacon\", "
						"\"data\": %s, "
						"\"upload\": %s, "
						"\"pairing\": %s, "
						"\"period\": %f, "
						"\"status\": %s, "
						"\"auth\": %s, "
						"}",
						(status1 & (1 << 5)) ? "true" : "false",
						(status1 & (1 << 4)) ? "true" : "false",
						(status1 & (1 << 3)) ? "true" : "false",
						period);
			}
			break;
		default:
			return false;
	}

	return true;
}
*/
