/*
============================================================
File : ObjBinBinaryReader.h

Project :
GPT DIVA FARC TOOL

Target :
Cinema 4D R19 / Visual Studio 2015

内容 :
OBJ.BIN / TEX.BIN 等の Little Endian バイナリ解析で
共通使用する BinaryReaderLE。

MikuMikuLibrary の EndianBinaryReader から、
今回の OBJ.BIN Material 解析に必要な機能を
C++ / C4D R19 用へ移植する。

重要 :
・データを変換しない
・float を勝手に Double 化して保存しない
・Offset の解釈は呼び出し側で決定する
・読み取り範囲を必ず検証する

Stage :
OBJ.BIN Native Binary Reader

今回やらないこと :
・Material生成
・Texture生成
・TEX.BIN解析
・座標変換
・色変換
・UV変換

次段階 :
Material / Texture の Native 接続
============================================================
*/

#ifndef GPT_DIVA_FARC_TOOL_OBJ_BIN_BINARY_READER_H__
#define GPT_DIVA_FARC_TOOL_OBJ_BIN_BINARY_READER_H__

#include "c4d.h"

#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

namespace GPTDiva
{
	namespace ObjBin
	{

		class BinaryReaderLE
		{
		private:

			const std::vector<UChar>& _data;


		public:

			explicit BinaryReaderLE(
				const std::vector<UChar>& data
			)
				: _data(data)
			{
			}


			UInt32 Size() const
			{
				if (_data.size() >
					static_cast<size_t>(0xFFFFFFFFULL))
				{
					return 0xFFFFFFFFU;
				}

				return static_cast<UInt32>(
					_data.size()
					);
			}


			Bool CanRead(
				UInt32 offset,
				UInt32 size
			) const
			{
				const UInt64 end =
					static_cast<UInt64>(offset) +
					static_cast<UInt64>(size);

				return end <=
					static_cast<UInt64>(_data.size());
			}


			const std::vector<UChar>&
				Data() const
			{
				return _data;
			}


			Bool ReadUInt32(
				UInt32 offset,
				UInt32& value
			) const
			{
				if (!CanRead(offset, 4))
					return false;

				value =
					static_cast<UInt32>(_data[offset + 0]) |
					(static_cast<UInt32>(_data[offset + 1]) << 8) |
					(static_cast<UInt32>(_data[offset + 2]) << 16) |
					(static_cast<UInt32>(_data[offset + 3]) << 24);

				return true;
			}


			Bool ReadInt32(
				UInt32 offset,
				Int32& value
			) const
			{
				UInt32 raw = 0;

				if (!ReadUInt32(offset, raw))
					return false;

				value =
					static_cast<Int32>(
						static_cast<std::int32_t>(raw)
						);

				return true;
			}


			Bool ReadUInt16(
				UInt32 offset,
				UInt16& value
			) const
			{
				if (!CanRead(offset, 2))
					return false;

				value =
					static_cast<UInt16>(_data[offset + 0]) |
					(static_cast<UInt16>(_data[offset + 1]) << 8);

				return true;
			}


			Bool ReadFloat32(
				UInt32 offset,
				Float32& value
			) const
			{
				if (!CanRead(offset, 4))
					return false;

				UInt32 raw = 0;

				if (!ReadUInt32(offset, raw))
					return false;

				std::memcpy(
					&value,
					&raw,
					sizeof(Float32)
				);

				return true;
			}


			Bool ReadBytes(
				UInt32 offset,
				UInt32 size,
				std::vector<UChar>& output
			) const
			{
				if (!CanRead(offset, size))
					return false;

				try
				{
					output.assign(
						_data.begin() + offset,
						_data.begin() + offset + size
					);
				}
				catch (...)
				{
					return false;
				}

				return true;
			}


			Bool ReadFixedString(
				UInt32 offset,
				UInt32 byteCount,
				std::string& value
			) const
			{
				if (!CanRead(offset, byteCount))
					return false;

				value.clear();

				for (
					UInt32 i = 0;
					i < byteCount;
					++i
					)
				{
					const UChar c =
						_data[offset + i];

					if (c == 0)
						break;

					value.push_back(
						static_cast<char>(c)
					);
				}

				return true;
			}
		};

	}
}

#endif