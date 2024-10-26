#pragma once
class CGenPacket
{
public:
{%- for pkt in scList %}
	static CSerializableBuffer *makePacket{{pkt.name}}({%- for arg in pkt.argList %} {{arg.type}} {{arg.name}}{{'' if loop.last else ','}}{%- endfor %});
{%- endfor %}
};

