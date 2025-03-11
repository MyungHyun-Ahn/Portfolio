#pragma once
class CEncryption
{
public:
	inline static void Encoding(char *msg, int len, int randKey) noexcept
	{
		char P = 0;
		char E = 0;
		int randKeyPlusOne = randKey + 1;
		int packetKeyPlusOne = PACKET_KEY + 1;
	
		int i = 0;
		for (; i <= len - 4; i += 4)
		{
			P = msg[i] ^ (P + randKeyPlusOne + i);
			E = P ^ (E + packetKeyPlusOne + i);
			msg[i] = E;
	
			P = msg[i + 1] ^ (P + randKeyPlusOne + i + 1);
			E = P ^ (E + packetKeyPlusOne + i + 1);
			msg[i + 1] = E;
	
			P = msg[i + 2] ^ (P + randKeyPlusOne + i + 2);
			E = P ^ (E + packetKeyPlusOne + i + 2);
			msg[i + 2] = E;
	
			P = msg[i + 3] ^ (P + randKeyPlusOne + i + 3);
			E = P ^ (E + packetKeyPlusOne + i + 3);
			msg[i + 3] = E;
		}
	
		for (; i < len; i++)
		{
			P = msg[i] ^ (P + randKeyPlusOne + i);
			E = P ^ (E + packetKeyPlusOne + i);
			msg[i] = E;
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

	inline static void Decoding(char *msg, int len, int randKey) noexcept
	{
		char prevP = 0;
		char prevE = 0;
		int randKeyPlusOne = randKey + 1;
		int packetKeyPlusOne = PACKET_KEY + 1;

		int i = 0;
		for (; i <= len - 4; i += 4)
		{
			char P = msg[i] ^ (prevE + packetKeyPlusOne + i);
			char D = P ^ (prevP + randKeyPlusOne + i);
			prevE = msg[i];
			msg[i] = D;
			prevP = P;

			P = msg[i + 1] ^ (prevE + packetKeyPlusOne + i + 1);
			D = P ^ (prevP + randKeyPlusOne + i + 1);
			prevE = msg[i + 1];
			msg[i + 1] = D;
			prevP = P;

			P = msg[i + 2] ^ (prevE + packetKeyPlusOne + i + 2);
			D = P ^ (prevP + randKeyPlusOne + i + 2);
			prevE = msg[i + 2];
			msg[i + 2] = D;
			prevP = P;

			P = msg[i + 3] ^ (prevE + packetKeyPlusOne + i + 3);
			D = P ^ (prevP + randKeyPlusOne + i + 3);
			prevE = msg[i + 3];
			msg[i + 3] = D;
			prevP = P;
		}

		for (; i < len; i++)
		{
			char P = msg[i] ^ (prevE + packetKeyPlusOne + i);
			char D = P ^ (prevP + randKeyPlusOne + i);
			prevE = msg[i];
			msg[i] = D;
			prevP = P;
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

	inline static BYTE CalCheckSum(char *msg, int len) noexcept
	{
		int sum = 0;
		int i = 0;

		// 루프 전개를 사용하여 4바이트씩 처리
		for (; i <= len - 4; i += 4)
		{
			sum += msg[i];
			sum += msg[i + 1];
			sum += msg[i + 2];
			sum += msg[i + 3];
		}

		// 남은 바이트 처리
		for (; i < len; i++)
		{
			sum += msg[i];
		}

		return (BYTE)(sum % 256);
	}

public:
	inline static BYTE PACKET_KEY;
};

