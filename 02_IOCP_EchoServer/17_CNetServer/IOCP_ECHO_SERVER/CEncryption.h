#pragma once
class CEncryption
{
public:
	static void Encoding(char *msg, int len, int randKey) noexcept
	{
		char prevP = 0;
		char prevE = 0;
		for (int i = 0; i < len; i++)
		{
			char P = msg[i] ^ (prevP + randKey + i + 1);
			char E = P ^ (prevE + FIXED_KEY + i + 1);
			msg[i] = E;
			prevP = P;
			prevE = E;
		}
	}

	static void Decoding(char *msg, int len, int randKey) noexcept
	{
		char prevP = 0;
		char prevE = 0;
		for (int i = 0; i < len; i++)
		{
			char P = msg[i] ^ (prevE + FIXED_KEY + i + 1);
			char D = P ^ (prevP + randKey + i + 1);
			prevE = msg[i];
			msg[i] = D;
			prevP = P;
		}
	}

	static BYTE CalCheckSum(char *msg, int len) noexcept
	{
		int sum = 0;
		for (int i = 0; i < len; i++)
		{
			sum += msg[i];
		}

		return (BYTE)(sum % 256);
	}

public:
	inline static BYTE FIXED_KEY;
};

