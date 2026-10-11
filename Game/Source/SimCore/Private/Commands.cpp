#include "Sim/Commands.h"

namespace Sim
{
	void WriteCommand(ByteWriter& W, const Command& C)
	{
		W.U32(C.Tick);
		W.U8(C.Player);
		W.U8(uint8_t(C.Type));
		switch (C.Type)
		{
		case CommandType::SetGroup:
			W.U16(uint16_t(C.Units.size()));
			for (const EntityId Id : C.Units)
			{
				W.U32(Id);
			}
			break;
		case CommandType::Move:
		case CommandType::AttackMove:
			W.I64(C.Point.X.Raw);
			W.I64(C.Point.Y.Raw);
			break;
		case CommandType::Attack:
			W.U32(C.Target);
			break;
		case CommandType::Stop:
			break;
		case CommandType::SetFog:
			W.U8(C.Target != 0 ? 1 : 0);
			break;
		case CommandType::Produce:
		case CommandType::CancelProduce:
			W.U32(C.Target);
			W.U32(uint32_t(C.Value));
			break;
		case CommandType::Build:
			W.U32(C.Target);
			W.U32(uint32_t(C.Value));
			W.I64(C.Point.X.Raw);
			W.I64(C.Point.Y.Raw);
			break;
		case CommandType::CancelBuild:
			W.U32(C.Target);
			break;
		case CommandType::SetRally:
			W.U32(C.Target);
			W.I64(C.Point.X.Raw);
			W.I64(C.Point.Y.Raw);
			break;
		case CommandType::Gather:
			W.U32(uint32_t(C.Value));
			break;
		}
	}

	bool ReadCommand(ByteReader& R, Command& Out)
	{
		Out = Command();
		uint8_t Type = 0;
		if (!R.U32(Out.Tick) || !R.U8(Out.Player) || !R.U8(Type))
		{
			return false;
		}
		if (Out.Player >= MaxPlayers)
		{
			return false;
		}
		Out.Type = CommandType(Type);
		switch (Out.Type)
		{
		case CommandType::SetGroup:
		{
			uint16_t Count = 0;
			if (!R.U16(Count) || Count > MaxGroupSize)
			{
				return false;
			}
			Out.Units.resize(Count);
			for (uint16_t I = 0; I < Count; ++I)
			{
				if (!R.U32(Out.Units[I]))
				{
					return false;
				}
			}
			return true;
		}
		case CommandType::Move:
		case CommandType::AttackMove:
			return R.I64(Out.Point.X.Raw) && R.I64(Out.Point.Y.Raw);
		case CommandType::Attack:
			return R.U32(Out.Target);
		case CommandType::Stop:
			return true;
		case CommandType::SetFog:
		{
			uint8_t On = 0;
			if (!R.U8(On) || On > 1)
			{
				return false;
			}
			Out.Target = On;
			return true;
		}
		case CommandType::Produce:
		case CommandType::CancelProduce:
		{
			uint32_t V = 0;
			if (!R.U32(Out.Target) || !R.U32(V) || V > 0xFFFF)
			{
				return false;
			}
			Out.Value = int32_t(V);
			return true;
		}
		case CommandType::Build:
		{
			uint32_t V = 0;
			if (!R.U32(Out.Target) || !R.U32(V) || V > 0x3FFFF || !R.I64(Out.Point.X.Raw) || !R.I64(Out.Point.Y.Raw))
			{
				return false;
			}
			Out.Value = int32_t(V);
			return true;
		}
		case CommandType::CancelBuild:
			return R.U32(Out.Target);
		case CommandType::SetRally:
			return R.U32(Out.Target) && R.I64(Out.Point.X.Raw) && R.I64(Out.Point.Y.Raw);
		case CommandType::Gather:
		{
			uint32_t V = 0;
			if (!R.U32(V) || V > 0xFFFF)
			{
				return false;
			}
			Out.Value = int32_t(V);
			return true;
		}
		}
		return false;   // tipo desconhecido
	}
}
