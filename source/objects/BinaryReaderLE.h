// File : BinaryReaderLE.h
// Project : GPT DIVA FARC TOOL
// Target : Cinema 4D R19 / Visual Studio 2015
//
// 内容:
//   OBJ.BIN / MikuMikuLibrary 解析で共通使用する
//   Little Endian Binary Reader。
//
//   現在 ObjBinAnalyzer.cpp に存在していた BinaryReaderLE を
//   独立ヘッダーへ移動する。
//   
//   ObjectSet / Object / Mesh / SubMesh / Position / Normal / UV
//   / Material / Texture / Skin / Bone / EX Data で共通使用する。
//
// Stage:
//   Common Binary Reader
//
// 今回やらないこと:
//   FARC 解凍
//   ObjectSet 解析
//   Material 解析
//   Texture 実体解析
//
// 次段階:
//   ObjBinMaterialAnalyzer.cpp から本 Reader を使用する。
// ============================================================

#ifndef GPT_DIVA_FARC_TOOL_BINARY_READER_LE_H
#define GPT_DIVA_FARC_TOOL_BINARY_READER_LE_H


#include <vector>
#include <string>
#include <cstring>


namespace GPTDiva
{
	namespace ObjBin
	{


		// ============================================================
		// BinaryReaderLE
		// ============================================================

		class BinaryReaderLE
		{
		private:

			const std::vector<UChar>&
				_data;


		public:

			// --------------------------------------------------------
			// Constructor
			// --------------------------------------------------------

			BinaryReaderLE(
				const std::vector<UChar>& data)
				: _data(data)
			{
			}


			// --------------------------------------------------------
			// Size
			// --------------------------------------------------------

			UInt32 Size() const
			{
				if (_data.size() >
					(size_t)0xFFFFFFFFULL)
				{
					return 0xFFFFFFFFU;
				}


				return
					(UInt32)_data.size();
			}


			// --------------------------------------------------------
			// CanRead
			// --------------------------------------------------------

			Bool CanRead(
				UInt32 offset,
				UInt32 size) const
			{
				if (offset > Size())
				{
					return false;
				}


				if (size > Size() - offset)
				{
					return false;
				}


				return true;
			}


			// --------------------------------------------------------
			// ReadUInt8
			// --------------------------------------------------------

			Bool ReadUInt8(
				UInt32 offset,
				UChar& value) const
			{
				if (!CanRead(
					offset,
					1))
				{
					return false;
				}


				value =
					_data[
						(size_t)offset
					];


				return true;
			}


			// --------------------------------------------------------
			// ReadUInt16
			// --------------------------------------------------------

			Bool ReadUInt16(
				UInt32 offset,
				UInt32& value) const
			{
				if (!CanRead(
					offset,
					2))
				{
					return false;
				}


				const UInt32 b0 =
					(UInt32)_data[
						(size_t)offset
					];


				const UInt32 b1 =
					(UInt32)_data[
						(size_t)offset + 1
					];


				value =
					b0 |
					(b1 << 8);


				return true;
			}


			// --------------------------------------------------------
			// ReadUInt32
			// --------------------------------------------------------

			Bool ReadUInt32(
				UInt32 offset,
				UInt32& value) const
			{
				if (!CanRead(
					offset,
					4))
				{
					return false;
				}


				const UInt32 b0 =
					(UInt32)_data[
						(size_t)offset
					];


				const UInt32 b1 =
					(UInt32)_data[
						(size_t)offset + 1
					];


				const UInt32 b2 =
					(UInt32)_data[
						(size_t)offset + 2
					];


				const UInt32 b3 =
					(UInt32)_data[
						(size_t)offset + 3
					];


				value =
					b0 |
					(b1 << 8) |
					(b2 << 16) |
					(b3 << 24);


				return true;
			}


			// --------------------------------------------------------
			// ReadFloat32
			// --------------------------------------------------------

			Bool ReadFloat32(
				UInt32 offset,
				Float32& value) const
			{
				UInt32 raw =
					0;


				if (!ReadUInt32(
					offset,
					raw))
				{
					return false;
				}


				float f =
					0.0f;


				std::memcpy(
					&f,
					&raw,
					sizeof(float)
				);


				value =
					(Float32)f;


				return true;
			}


			// --------------------------------------------------------
			// ReadFixedString
			// --------------------------------------------------------

			Bool ReadFixedString(
				UInt32 offset,
				UInt32 size,
				std::string& value) const
			{
				if (!CanRead(
					offset,
					size))
				{
					return false;
				}


				value.clear();


				for (UInt32 i = 0;
					i < size;
					++i)
				{
					const unsigned char c =
						(unsigned char)_data[
							(size_t)offset + i
						];


					if (c == 0)
					{
						break;
					}


					value.push_back(
						(char)c
					);
				}


				return true;
			}
		};


	}
}


#endif