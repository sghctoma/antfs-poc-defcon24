#include <Pothos/Framework.hpp>
#include "ShockBurstUtils.hpp"
#include <iostream>
#include <cmath>

/***********************************************************************
 * |PothosDoc  ShockBurst Decoder
 *
 * Decode ShockBurst packets.
 * The decoder block accepts a stream of real-valued samples on input port 0,
 * and produces a message containing ShockBurst packets on output port 0.
 *
 * <h2>Input format</h2>
 *
 * The input port expects either signed integers that have been frequency
 * demodulated or alternatively, frequency demodulated floating point samples
 * between -pi and +pi. The scaling of the input samples does not matter, and
 * the input sample rate should be 2 Msps. A typical upstream flow involves raw
 * complex baseband samples and the "Freq Demod" block.
 *
 * <h2>Output format</h2>
 *
 * Each decoded packet results in a message of type ShockBurst that contains the
 * address, payload and CRC fields of a ShockBurst packet.
 *
 * |category /Decode
 * |keywords shockburst
 *
 * |param addressLength[Address Length] The length of the address field in bytes
 * (3-5).
 * |option [3] 3
 * |option [4] 4
 * |option [5] 5
 * |default 5
 *
 * |param payloadLength[Payload Length] The length of the packet payload in
 * bytes (1-32).
 * |widget SpinBox(minimum=1,maximum=32)
 * |default 10
 *
 * |param crcLength[CRC Length (NOT IMPLEMENTED YET)] The length of the CRC
 * field in bytes (1-2).
 * |option [CRC8] 1
 * |option [CRC16] 2
 * |default 2
 *
 * |factory /shockburst/shockburst_decoder()
 * |initializer setAddressLength(addressLength)
 * |initializer setPayloadLength(payloadLength)
 * |initializer setCRCLength(crcLength)
 **********************************************************************/
class ShockBurstDecoder : public Pothos::Block
{
public:
	ShockBurstDecoder(void)
	{
		this->setupInput(0); //unspecified type, handles conversion
		this->setupOutput(0);
		
		this->registerCall(this, POTHOS_FCN_TUPLE(ShockBurstDecoder, setAddressLength));
		this->registerCall(this, POTHOS_FCN_TUPLE(ShockBurstDecoder, getAddressLength));
		this->registerCall(this, POTHOS_FCN_TUPLE(ShockBurstDecoder, setPayloadLength));
		this->registerCall(this, POTHOS_FCN_TUPLE(ShockBurstDecoder, getPayloadLength));
		this->registerCall(this, POTHOS_FCN_TUPLE(ShockBurstDecoder, setCRCLength));
		this->registerCall(this, POTHOS_FCN_TUPLE(ShockBurstDecoder, getCRCLength));
		
		this->setAddressLength(5);
		this->setPayloadLength(10);
		this->setCRCLength(2);

		_decoder = new ShockBurstUtilsDecoder(_addressLength, _payloadLength);
	}

	~ShockBurstDecoder(void)
	{
		delete _decoder;
	}

	static Block *make(void)
	{
		return new ShockBurstDecoder();
	}

	void work(void)
	{
		auto inPort = this->input(0);
		auto inBuff = inPort->buffer();
		auto N = inBuff.elements();
		if (N == 0) return; //nothing available

		//floating point support
		if (inBuff.dtype.isFloat())
		{
			auto float32Buff = inBuff.convert(typeid(float));
			auto in = float32Buff.as<const float *>();
			const float gain = (1 << 15)/M_PI;
			for (size_t i = 0; i < N; i++)
			{
				if (_decoder->feedOne(uint16_t(in[i]*gain)))
				{
					this->output(0)->postMessage(_decoder->packetData);
				}
			}
		}

		//fixed point support
		else
		{
			auto int16Buff = inBuff.convert(typeid(int16_t));
			auto in = int16Buff.as<const uint16_t *>();
			for (size_t i = 0; i < N; i++)
			{
				if (_decoder->feedOne(in[i]))
				{
					this->output(0)->postMessage(_decoder->packetData);
				}
			}
		}

		//consume all input elements
		inPort->consume(inPort->elements());
	}

	void setAddressLength(const uint8_t &addressLength)
	{
		_addressLength = addressLength;
	}

	uint8_t getAddressLength(void) const
	{
		return _addressLength;
	}

	void setPayloadLength(const uint8_t &payloadLength)
	{
		_payloadLength = payloadLength;
	}

	uint8_t getPayloadLength(void) const
	{
		return _payloadLength;
	}

	void setCRCLength(const uint8_t &crcLength)
	{
		_crcLength = crcLength;
	}

	uint8_t getCRCLength(void) const
	{
		return _crcLength;
	}

private:
	uint8_t _addressLength;
	uint8_t _payloadLength;
	uint8_t _crcLength;
	ShockBurstUtilsDecoder *_decoder;
};

static Pothos::BlockRegistry registerShockBurstDecoder(
	"/shockburst/shockburst_decoder", &ShockBurstDecoder::make);
