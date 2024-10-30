#include "pch.h"
#include "DefinePacket.h"
#include "CGenPacket.h"

{%- for pkt in scList %}
CSerializableBuffer *CGenPacket::makePacket{{pkt.name}}({%- for arg in pkt.argList %} {{arg.type}} {{arg.name}}{{'' if loop.last else ','}}{%- endfor %})
{
#ifdef USE_OBJECT_POOL
	CSerializableBuffer *pSBuffer = CSerializableBuffer::Alloc();
#else
	CSerializableBuffer *pSBuffer = new CSerializableBuffer;
#endif
	BYTE type = (BYTE)PACKET_CODE::{{pkt.name}};
	*pSBuffer << type << {%- for arg in pkt.argList %} {{arg.name}}{{';' if loop.last else ' << '}}{%- endfor %}
	return pSBuffer;
} {{'' if loop.last else '\n'}}
{%- endfor %}
