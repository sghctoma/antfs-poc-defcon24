#pragma once
#include <Pothos/Object/Containers.hpp>
#include <Poco/Format.h>
#include <chrono>
#include <cstdint>
#include <cctype>
#include <iomanip>
#include <string>
#include <sstream>

/*
class RingBuffer
{
private:
	int _head;
	size_t _size;
	int16_t *_buffer;

public:
	RingBuffer(size_t size):
		_size(size),
		_head(-1),
		_buffer(new int16_t[2 * _size])
	{ }

	~RingBuffer()
	{
		delete [] _buffer;
	}

	inline void increment(void)
	{
		++_head;
		_head = _head % _size;
	}

	inline int16_t get(size_t index)
	{
		return _buffer[(_head + index) % _size];
	}

	inline void put(int16_t value)
	{
		_buffer[_head % _size] = value;
	}
};
*/

class ShockBurstUtilsDecoder
{
private:
	int _skip;
	int32_t _samples;
	int32_t _threshold;
	//XXX: RingBuffer _ringbuffer;
	
	const uint8_t ADDRESS_LENGTH;
	const uint8_t PAYLOAD_LENGTH;

	//XXX: **** RingBuffer
	const size_t RB_SIZE = 1000;
	int rb_head = -1;
	int16_t *rb_buf;

	void RB_init(void)
	{
		rb_buf = (int16_t *)malloc(RB_SIZE * 2);
	}

	void RB_inc(void)
	{
		rb_head++;
		rb_head=(rb_head) % RB_SIZE;
	}
	#define RB(l) rb_buf[(rb_head+(l))%RB_SIZE]
	//****

	/*
	* Quantize sample at location l by checking whether it's over the threshold.
	* Important - takes into account the sample rate downconversion ratio!
	*/
	inline bool quantize(int16_t l)
	{
		//XXX: return _ringbuffer.get(l * 2) > _threshold;
		return RB(l * 2) > _threshold;
	}

	/* Extract quantization threshold from preamble sequence */
	int32_t extractThreshold(void)
	{
		int32_t threshold = 0;
		int c;
		for (c = 0; c < 16 /* 2 * srate */; c++) {
			//XXX: threshold += (int32_t)_ringbuffer.get(c);
			threshold += (int32_t)RB(c);
		}

		return (int32_t)threshold / 16; // 8 * g_srate
	}

	/* Identify preamble sequence */
	bool detectPreamble(void)
	{
		int transitions = 0;
		int c;

		// preamble sequence is based on the 9th symbol (either 0x55 or 0xAA)
		if (quantize(9)) {
			for (c = 0; c < 8; c++) {
				transitions += quantize(c) > quantize(c + 1);
			}
		} else {
			for (c = 0; c < 8; c++) {
				transitions += quantize(c) < quantize(c + 1);
			}
		}

		return transitions == 4 && abs(_threshold) < 15500;
	}

	/* Extract byte from ring buffer starting location l */
	inline uint8_t extractByte(int l)
	{
		uint8_t byte = 0;
		int c;
		for (c = 0; c < 8; c++) {
			byte |= quantize(l + c) << (7 - c);
		}

		return byte;
	}

	/* Extract count bytes from ring buffer starting location l into buffer*/
	inline void extractBytes(int l, uint8_t* buffer, int count)
	{
		int t;
		for (t = 0; t < count; t++) {
			buffer[t] = extractByte(l + t * 8);
		}
	}

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

	bool isShockBurstPacket(uint8_t* bytes)
	{
		uint16_t packet_crc, calced_crc;

		calced_crc = crc16(bytes, ADDRESS_LENGTH + PAYLOAD_LENGTH);
		packet_crc = bytes[ADDRESS_LENGTH + PAYLOAD_LENGTH] << 8 |
			bytes[ADDRESS_LENGTH + PAYLOAD_LENGTH + 1];

		return calced_crc == packet_crc;
	}

	/*
	* ShockBurst packet format (length in bytes in parenthesis):
	*
	* +--------------+---------------+----------------------------+-----------+
	* | Preamble (1) | Address (3-5) | Payload (1-32)             | CRC (1-2) |
	* +--------------+---------------+----------------------------+-----------+
	*
	* CRC is calculated over the "Address" and "Payload" fields, and it is 2
	* bytes in case of ANT.
	*/
	bool decodePacket(int32_t sample)
	{
		int i;
		_threshold = extractThreshold();

		if (detectPreamble()) {
			uint8_t tmp_buf[ADDRESS_LENGTH + PAYLOAD_LENGTH + 2];
			extractBytes(8, tmp_buf, sizeof(tmp_buf));
			if (isShockBurstPacket(tmp_buf)) {
				packetData.clear();

				// address
				uint64_t address = 0;
				for (int i = 0; i < ADDRESS_LENGTH; ++i)
					address |= tmp_buf[i] << (ADDRESS_LENGTH - 1 - i) * 8;
				packetData["address"] = Pothos::Object(address);

				// CRC
				uint16_t crc = tmp_buf[ADDRESS_LENGTH + PAYLOAD_LENGTH] << 8 |
					tmp_buf[ADDRESS_LENGTH + PAYLOAD_LENGTH + 1];
				packetData["crc"] = Pothos::Object(crc);

				// paylod
				packetData["payload"] = Pothos::Object(std::vector<uint8_t>(
							tmp_buf + ADDRESS_LENGTH,
							tmp_buf + ADDRESS_LENGTH + PAYLOAD_LENGTH));

				return true;
			}
		}

		return false;
	}

public:
	Pothos::ObjectKwargs packetData;

	ShockBurstUtilsDecoder(uint8_t addressLength, uint8_t payloadLength):
		//XXX: _ringbuffer(1000),
		_threshold(0),
		_samples(0),
		_skip(1000),
		ADDRESS_LENGTH(addressLength),
		PAYLOAD_LENGTH(payloadLength)
	{
		RB_init();
	}

	bool feedOne(const uint16_t sample)
	{
		//XXX: _ringbuffer.increment();
		//_ringbuffer.put((int)sample);
		RB_inc();
		RB(0) = (int)sample;

		if (--_skip < 1) {
			if (decodePacket(++_samples)) {
				_skip = 20;
				return true;
			}
		}
		return false;
	}
};
