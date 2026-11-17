#pragma once

namespace MHLib::security
{
class CEncryption
{
public:
	inline static void Encoding(char *pMsg, int len, int randKey) noexcept
	{
		char plainByte = 0;
		char encodedByte = 0;
		int randKeyPlusOne = randKey + 1;
		int packetKeyPlusOne = sm_packetKey + 1;
	
		int i = 0;
		for (; i <= len - 4; i += 4)
		{
			plainByte = pMsg[i] ^ (plainByte + randKeyPlusOne + i);
			encodedByte = plainByte ^ (encodedByte + packetKeyPlusOne + i);
			pMsg[i] = encodedByte;
	
			plainByte = pMsg[i + 1] ^ (plainByte + randKeyPlusOne + i + 1);
			encodedByte = plainByte ^ (encodedByte + packetKeyPlusOne + i + 1);
			pMsg[i + 1] = encodedByte;
	
			plainByte = pMsg[i + 2] ^ (plainByte + randKeyPlusOne + i + 2);
			encodedByte = plainByte ^ (encodedByte + packetKeyPlusOne + i + 2);
			pMsg[i + 2] = encodedByte;
	
			plainByte = pMsg[i + 3] ^ (plainByte + randKeyPlusOne + i + 3);
			encodedByte = plainByte ^ (encodedByte + packetKeyPlusOne + i + 3);
			pMsg[i + 3] = encodedByte;
		}
	
		for (; i < len; i++)
		{
			plainByte = pMsg[i] ^ (plainByte + randKeyPlusOne + i);
			encodedByte = plainByte ^ (encodedByte + packetKeyPlusOne + i);
			pMsg[i] = encodedByte;
		}
	}

	// inline static void Encoding(char *msg, int len, int randKey) noexcept
	// {
	// 	char P = 0;
	// 	char E = 0;
	// 	int randKeyPlusOne = randKey + 1;
	// 	int packetKeyPlusOne = PACKET_KEY + 1;
	// 
	// 	for (int i = 0; i < len; i++)
	// 	{
	// 		int iPlusOne = i + 1;
	// 		P = msg[i] ^ (P + randKeyPlusOne + i);
	// 		E = P ^ (E + packetKeyPlusOne + i);
	// 		msg[i] = E;
	// 	}
	// }

	inline static void Decoding(char *pMsg, int len, int randKey) noexcept
	{
		char prevP = 0;
		char prevE = 0;
		int randKeyPlusOne = randKey + 1;
		int packetKeyPlusOne = sm_packetKey + 1;

		int i = 0;
		for (; i <= len - 4; i += 4)
		{
			char plainByte = pMsg[i] ^ (prevE + packetKeyPlusOne + i);
			char decodedByte = plainByte ^ (prevP + randKeyPlusOne + i);
			prevE = pMsg[i];
			pMsg[i] = decodedByte;
			prevP = plainByte;

			plainByte = pMsg[i + 1] ^ (prevE + packetKeyPlusOne + i + 1);
			decodedByte = plainByte ^ (prevP + randKeyPlusOne + i + 1);
			prevE = pMsg[i + 1];
			pMsg[i + 1] = decodedByte;
			prevP = plainByte;

			plainByte = pMsg[i + 2] ^ (prevE + packetKeyPlusOne + i + 2);
			decodedByte = plainByte ^ (prevP + randKeyPlusOne + i + 2);
			prevE = pMsg[i + 2];
			pMsg[i + 2] = decodedByte;
			prevP = plainByte;

			plainByte = pMsg[i + 3] ^ (prevE + packetKeyPlusOne + i + 3);
			decodedByte = plainByte ^ (prevP + randKeyPlusOne + i + 3);
			prevE = pMsg[i + 3];
			pMsg[i + 3] = decodedByte;
			prevP = plainByte;
		}

		for (; i < len; i++)
		{
			char plainByte = pMsg[i] ^ (prevE + packetKeyPlusOne + i);
			char decodedByte = plainByte ^ (prevP + randKeyPlusOne + i);
			prevE = pMsg[i];
			pMsg[i] = decodedByte;
			prevP = plainByte;
		}
	}

	// inline static void Decoding(char *msg, int len, int randKey) noexcept
	// {
	// 	char prevP = 0;
	// 	char prevE = 0;
	// 	int randKeyPlusOne = randKey + 1;
	// 	int packetKeyPlusOne = PACKET_KEY + 1;
	// 
	// 	for (int i = 0; i < len; i++)
	// 	{
	// 		int iPlusOne = i + 1;
	// 		char P = msg[i] ^ (prevE + packetKeyPlusOne + i);
	// 		char D = P ^ (prevP + randKeyPlusOne + i);
	// 		prevE = msg[i];
	// 		msg[i] = D;
	// 		prevP = P;
	// 	}
	// }

	// inline static BYTE CalCheckSum(char *msg, int len) noexcept
	// {
	// 	int sum = 0;
	// 	for (int i = 0; i < len; i++)
	// 	{
	// 		sum += msg[i];
	// 	}
	// 
	// 	return (BYTE)(sum % 256);
	// }

	inline static BYTE CalCheckSum(char *pMsg, int len) noexcept
	{
		int sum = 0;
		int i = 0;

		// 루프 전개를 사용하여 4바이트씩 처리
		for (; i <= len - 4; i += 4)
		{
			sum += pMsg[i];
			sum += pMsg[i + 1];
			sum += pMsg[i + 2];
			sum += pMsg[i + 3];
		}

		// 남은 바이트 처리
		for (; i < len; i++)
		{
			sum += pMsg[i];
		}

		return (BYTE)(sum % 256);
	}

public:
	inline static BYTE sm_packetKey;
};
}
