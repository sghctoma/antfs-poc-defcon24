#include <Pothos/Framework.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <sstream>
#include <iomanip>

/***********************************************************************
 * |PothosDoc  ANT-FS Decoder
 *
 * Decode ANT-FS packets.
 * The decoder block accepts a stream of messages containing keyword value pairs
 * that represent ShockBurst packages on input port 0, and produces a message
 * containing keyword value pairs that represent an ANT-FS packet on output port
 * 0.
 *
 * <h2>Input format</h2>
 *
 * The input port expects messages that contain keyword value pairs that
 * represent ShockBurst packages, and tries to parse the "payload" field as
 * ANT-FS messages. A typical upstream flow involves raw complex baseband
 * samples, the "Freq Demod" and the "ShockBurst Decored" blocks.
 *
 * <h2>Output format</h2>
 *
 * Each decoded ANT-FS packet results in a dictionary message of
 * type Pothos::ObjectKwargs. The keyword and value pairs correspond with the
 * fields in the ANT-FS packet.
 *
 * |category /Decode
 * |keywords ant antfs ant-fs
 *
 * |param beaconChannel[Beacon Channel] The channel of the ANT-FS client
 * beacon. This value ranges from 3 Mhz to 80 Mhz in 1 Mhz steps, and the real
 * frequency is this value plus 2.4 GHz.
 * |widget SpinBox(minimum=3,maximum=80)
 * |default 50
 *
 * |factory /antfs/antfs_decoder()
 * |initializer setBeaconChannel(beaconChannel)
 **********************************************************************/
class ANTFSDecoder : public Pothos::Block
{
public:
	Pothos::ObjectKwargs packet;

	ANTFSDecoder(void)
	{
		this->setupInput(0); //unspecified type, handles conversion
		this->setupOutput(0);
		this->registerCall(this, POTHOS_FCN_TUPLE(ANTFSDecoder, setBeaconChannel));

		// Send signal about frequency change due to received Link or Diconnect
		// command. This will tipically be connected to the setFrequency slot of
		// the source device.
		this->registerSignal("frequencyChanged");

		this->setBeaconChannel(50);
	}

	static Block *make(void)
	{
		return new ANTFSDecoder();
	}

	void work(void)
	{
		auto input = this->input(0);
		if (not input->hasMessage()) return;

		auto msg = input->popMessage();
		if (msg.type() == typeid(Pothos::ObjectKwargs)) {
			auto contents = msg.extract<Pothos::ObjectKwargs>();
			auto payload = contents["payload"];
			if (payload.type() == typeid(std::vector<uint8_t>)) {
				packet.clear();

				std::vector<uint8_t> data = payload.extract<std::vector<uint8_t> >();
				switch(data[2]) {
					case 0x43:
						parseBeacon(data);
						break;
					case 0x44:
						parseCommandReponse(data);
						break;
					default:
						packet["type"] = Pothos::Object("unknown");
						packet["data"] = Pothos::Object(bytesToHex(data));
						break;
				}

				this->output(0)->postMessage(packet);
			}
		}
	}

	void setBeaconChannel(const uint32_t &beaconChannel)
	{
		_beaconChannel = beaconChannel;
		this->callVoid("frequencyChanged", 2400 + _beaconChannel);
	}

	uint32_t getBeaconChannel()
	{
		return _beaconChannel;
	}

private:
	uint32_t _beaconChannel;

	uint16_t makeWord(std::vector<uint8_t>& data, size_t start)
	{
		return data[start + 1] << 8 | data[start];
	}

	uint32_t makeDword(std::vector<uint8_t>& data, size_t start)
	{
		return data[start + 3] << 24 | data[start + 2] << 16 |
			data[start + 1] << 8 | data[start];
	}

	std::string bytesToHex(std::vector<uint8_t>& data)
	{
		std::ostringstream ss;
		ss << std::hex << std::setfill('0') << std::uppercase;
		for (auto &byte : data)
			ss << std::setw(2) << static_cast<int>(byte) << " ";

		return ss.str();
	}

	void parseCommandReponse(std::vector<uint8_t>& data)
	{
		switch(data[3]) {
			// ANTFS Commands
			case 0x02: parseLinkCommand(data); break;
			case 0x03: parseDisconnectCommand(data); break;
			case 0x04: parseAuthCommand(data); break;
			case 0x05: parsePingCommand(data); break;
			case 0x09: parseDownloadReqCommand1(data); break;
			case 0x0a: parseUploadReqCommand1(data); break;
			case 0x0b: parseEraseReqCommand(data); break;
			case 0x0c: parseUploadDataCommand(data); break;
			// ANTFS Responses
			case 0x84: parseAuthResponse(data); break;
			case 0x89: parseDownloadReqResp2(data); break;
			case 0x8a: parseUploadReqResp2(data); break;
			case 0x8b: parseEraseResp2(data); break;
			case 0x8c: parseUploadDataResp2(data); break;
			// None of the known commands/responses
			default:
				packet["type"] = Pothos::Object("unknown command/response");
				packet["data"] = Pothos::Object(bytesToHex(data));
				break;
		}
	}

	/*
	 * ANT-FS Beacon
	 */
	void parseBeacon(std::vector<uint8_t>& data)
	{
		packet["type"] = Pothos::Object("client beaecon");

		uint8_t status1 = data[3];
		packet["data"] = Pothos::Object(status1 & (1 << 5) ? "true" : "false");
		packet["upload"] = Pothos::Object(status1 & (1 << 4) ? "true" : "false");
		packet["pairing"] = Pothos::Object(status1 & (1 << 3) ? "true" : "false");

		std::string temp;
		switch (status1 & 7) {
			case 0: temp = "0.5 Hz (65535)"; break;
			case 1: temp = "1 Hz (32768)"; break;
			case 2: temp = "2 Hz (16384)"; break;
			case 3: temp = "4 Hz (8192)"; break;
			case 4: temp = "8 Hz (4096)"; break;
			case 7: temp = "match established"; break;
			default: temp = "reserved";
		}
		packet["period"] = Pothos::Object(temp);

		switch (data[4] & 0x0f) {
			case 0:
				temp = "link";
				packet["device_type"] = Pothos::Object(makeWord(data, 6));
				packet["manufacturer"] = Pothos::Object(makeWord(data, 8));
				break;
			case 1:
				temp = "auth";
				packet["host_serial"] = Pothos::Object(makeDword(data, 6));
				break;
			case 2:
				temp = "transport";
				packet["host_serial"] = Pothos::Object(makeDword(data, 6));
				break;
			case 3: temp = "busy"; break;
			default: temp = "reserved"; break;
		}
		packet["state"] = Pothos::Object(temp);

		switch (data[5]) {
			case 0: temp = "pass-through"; break;
			case 1: temp = "n/a"; break;
			case 2: temp = "pairing"; break;
			case 3: temp = "passkey & pairing"; break;
			default: temp = "reserved"; break;
		}
		packet["auth_type"] = Pothos::Object(temp);
	}

	/*
	 * ANT-FS Commands
	 */
	void parseLinkCommand(std::vector<uint8_t> data)
	{
		packet["type"] = Pothos::Object("link command");
		packet["frequency"] = Pothos::Object(data[4]);

		std::string temp;
		switch(data[5]) {
			case 0: temp = "0.5 Hz (65535)"; break;
			case 1: temp = "1 Hz (32768)"; break;
			case 2: temp = "2 Hz (16384)"; break;
			case 3: temp = "4 Hz (8192)"; break;
			case 4: temp = "8 Hz (4096)"; break;
			case 7: temp = "match established"; break;
			default: temp = "reserved";
		}
		packet["period"] = Pothos::Object(temp);

		packet["host_serial"] = Pothos::Object(makeDword(data, 6));

		// From ANT_File_Share_Technology.pdf: The host may use the Link command
		// to specify a different channel period or RF frequency for subsequent
		// interactions.
		this->callVoid("frequencyChanged", 2400 + data[4]);
	}

	void parseDisconnectCommand(std::vector<uint8_t> data)
	{	
		packet["type"] = Pothos::Object("disconnect command");

		std::string temp;
		if (data[4] == 0) temp = "return to link";
		else if (data[4] == 1) temp = "return to broadcast";
		else if (data[4] <= 127) temp = "reserved";
		else temp = "device specific";
		packet["disconnect_type"] = Pothos::Object(temp);

		packet["time_duration"] = Pothos::Object(data[5]);
		packet["application_duration"] = Pothos::Object(data[6]);

		// Connection state returns to Link layer, thus frequency changes back
		// to the initial Beacon Channel frequency.
		this->callVoid("frequencyChanged", 2400 + _beaconChannel);
	}
	
	void parseAuthCommand(std::vector<uint8_t> data)
	{
		packet["type"] = Pothos::Object("auth command");
	
		std::string temp;
		switch (data[4]) {
			case 0: temp = "pass-through"; break;
			case 1: temp = "request serial"; break;
			case 2: temp = "request pairing"; break;
			case 3: temp = "request passkey"; break;
			default: temp = "EINVAL"; break;
		}
		packet["auth_type"] = Pothos::Object(temp);

		packet["auth_string_length"] = Pothos::Object(data[5]);
		packet["host_serial"] = Pothos::Object(makeDword(data, 6));
	}
	
	void parsePingCommand(std::vector<uint8_t> data)
	{
		packet["type"] = Pothos::Object("ping command");
	}
	
	//TODO: 2-packet burst
	void parseDownloadReqCommand1(std::vector<uint8_t> data)
	{
		packet["type"] = Pothos::Object("download request command");
		packet["index"] = Pothos::Object(makeWord(data, 4));
		packet["offset"] = Pothos::Object(makeDword(data, 6));
	}
	
	//TODO: 2-packet burst
	void parseUploadReqCommand1(std::vector<uint8_t> data)
	{
		packet["type"] = Pothos::Object("upload request command");
		packet["index"] = Pothos::Object(makeWord(data, 4));
		packet["max_size"] = Pothos::Object(makeDword(data, 6));
	}
	
	//TODO: 2+n-packet burst
	void parseUploadDataCommand(std::vector<uint8_t> data)
	{
		packet["type"] = Pothos::Object("upload data command");
		packet["crc_seed"] = Pothos::Object(makeWord(data, 4));
		packet["offset"] = Pothos::Object(makeDword(data, 6));
	}
	
	void parseEraseReqCommand(std::vector<uint8_t> data)
	{
		packet["type"] = Pothos::Object("erase request command");
		packet["index"] = Pothos::Object(makeWord(data, 4));
	}
	
	/*
	 * ANT-FS Responses
	 */
	void parseAuthResponse(std::vector<uint8_t> data)
	{
		packet["type"] = Pothos::Object("auth response");

		std::string temp;
		switch (data[4]) {
			case 0: temp = "response to serial req."; break;
			case 1: temp = "accept"; break;
			case 2: temp = "reject"; break;
			default: temp = "EINVAL";
		}
		packet["response"] = Pothos::Object(temp);

		packet["auth_string_length"] = Pothos::Object(data[5]);
		packet["client_serial"] = Pothos::Object(makeDword(data, 6));
	}
	
	//TODO: 3+n-packet burst
	void parseDownloadReqResp2(std::vector<uint8_t> data)
	{
		packet["type"] = Pothos::Object("download request response");

		std::string temp;
		switch (data[4]) {
			case 0: temp = "ANTFS_OK"; break;
			case 1: temp = "ANTFS_ENOENT"; break;
			case 2: temp = "ANTFS_EACCESS"; break;
			case 3: temp = "ANTFS_ENOTREADY"; break;
			case 4: temp = "ANTFS_EINVAL"; break;
			case 5: temp = "ANTFS_ECRC"; break;
			default: temp = "EINVAL";
		}
		packet["response"] = Pothos::Object(temp);

		packet["remaining"] = Pothos::Object(makeDword(data, 6));
	}
	
	//TODO: 4-packet burst
	void parseUploadReqResp2(std::vector<uint8_t> data)
	{
		packet["type"] = Pothos::Object("upload request response");

		std::string temp;
		switch (data[4]) {
			case 0: temp = "ANTFS_OK"; break;
			case 1: temp = "ANTFS_ENOENT"; break;
			case 2: temp = "ANTFS_EACCESS"; break;
			case 3: temp = "ANTFS_ENOSPC"; break;
			case 4: temp = "ANTFS_EINVAL"; break;
			case 5: temp = "ANTFS_ENOTREADY"; break;
			default: temp = "EINVAL";
		}
		packet["response"] = Pothos::Object(temp);

		packet["last_offset"] = Pothos::Object(makeDword(data, 6));
	}

	//TODO: 2-packet burst
	void parseUploadDataResp2(std::vector<uint8_t> data)
	{
		packet["type"] = Pothos::Object("upload data response");
		packet["response"] = Pothos::Object(data[4] ? "true" : "false");
	}

	//TODO: 2-packet burst
	void parseEraseResp2(std::vector<uint8_t> data)
	{
		packet["type"] = Pothos::Object("erase response");

		std::string temp;
		switch (data[4]) {
			case 0: temp = "OK"; break;
			case 1: temp = "FAILED"; break;
			case 2: temp = "ENOTREADY"; break;
			default: temp = "EINVAL";
		}
		packet["response"] = Pothos::Object(temp);
	}
};

static Pothos::BlockRegistry registerANTFSDecoder(
		"/antfs/antfs_decoder", &ANTFSDecoder::make);
