#include <windows.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <png.h>
#include <format>
#include <regex>
#include "output.hpp"
#include "image_output_ks.h"
#include "version.hpp"

struct PIXEL_BGR {
	unsigned char b;
	unsigned char g;
	unsigned char r;
};

struct ERROR_MGR {
	struct jpeg_error_mgr jerr;
	jmp_buf jmpbuf;
};
static void
error_exit(j_common_ptr cinfo)
{
	longjmp((reinterpret_cast<ERROR_MGR *>(cinfo->err))->jmpbuf, 1);
}

enum OUTPUT_FORMAT {
	OF_JPEG,
	OF_PNG,
	OF_END
};

constexpr static const std::string default_format = "{1:s}_{0:04}";
struct CONFIG {
	OUTPUT_FORMAT output;
	int jpeg_quality;
	int offset;
	std::string fmt;
};
static CONFIG config = {OF_JPEG, 75, 0, default_format};
struct CONFIG_NUM {
	OUTPUT_FORMAT output;
	int jpeg_quality;
	int offset;
};

static std::wstring
Utf8ToUtf16(const std::string &utf8)
{
	if (utf8.empty()) return L"";
	int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
	std::wstring wstr(size - 1, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wstr.data(), size);
	return wstr;
}
static std::string
Utf16ToCp932(const std::wstring &utf16)
{
	if (utf16.empty()) return "";
	int size = WideCharToMultiByte(932, 0, utf16.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string str(size - 1, '\0');
	WideCharToMultiByte(932, 0, utf16.c_str(), -1, str.data(), size, nullptr, nullptr);
	return str;
}
static std::string
Utf8ToCp932(const std::string &utf8)
{
	return Utf16ToCp932(Utf8ToUtf16(utf8));
}
static std::wstring
Cp932ToUtf16(const std::string &sjis)
{
	if (sjis.empty()) return L"";
	int size = MultiByteToWideChar(932, 0, sjis.c_str(), -1, nullptr, 0);
	std::wstring wstr(size - 1, L'\0');
	MultiByteToWideChar(932, 0, sjis.c_str(), -1, wstr.data(), size);
	return wstr;
}
static std::string
Utf16ToUtf8(const std::wstring &utf16)
{
	if (utf16.empty()) return "";
	int size = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string str(size - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, str.data(), size, nullptr, nullptr);
	return str;
}
static std::string Cp932ToUtf8(const std::string &sjis)
{
	return Utf16ToUtf8(Cp932ToUtf16(sjis));
}

#define PLUGIN_NAME "連番画像出力"

EXTERN_C OUTPUT_PLUGIN_TABLE *
GetOutputPluginTable()
{
	static std::string plugin_name = Utf8ToCp932(PLUGIN_NAME);
	static CHAR filefilter[] = "JPEG File (*.jpg)\0*.jpg\0PNG File (*.png)\0*.png\0All File (*.*)\0*.*\0";
	static std::string information = Utf8ToCp932(PLUGIN_NAME " " VERSION " by KAZOON");
	static OUTPUT_PLUGIN_TABLE opt = {
		0,
		const_cast<LPSTR>(plugin_name.c_str()),
		filefilter,
		const_cast<LPSTR>(information.c_str()),
		nullptr, // func_init
		nullptr, // func_exit
		func_output,
		func_config,
		func_config_get,
		func_config_set,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 // reserve
	};
	return &opt;
}

static BOOL put_jpeg_file(FILE *fp, const unsigned char *video);
static BOOL put_png_file(FILE *fp, const png_bytep video);

static JSAMPROW row=nullptr;
static png_bytepp rows=nullptr;
static int width, height, dib_width;

static BOOL
finish_func_output(const BOOL &ret)
{
	free(row);
	row = nullptr;
	free(rows);
	rows = nullptr;
	return ret;
}
BOOL
func_output(OUTPUT_INFO *oip)
{
	std::string dir, name, ext, buff;
	std::string savefile = Cp932ToUtf8(std::string(oip->savefile));
	std::smatch m;
	if ( std::regex_match(savefile, m, std::regex(R"((.*[\\/])([^\\/]*)(\..+?)$)")) ) {
		dir = m[1].str();
		name = m[2].str();
		ext = m[3].str();
	} else {
		return FALSE;
	}
	
	if ( config.output == OF_JPEG ) {
		row = static_cast<JSAMPROW>( calloc(oip->w*3, sizeof(JSAMPLE)) );
		if ( row == nullptr ) { return FALSE; }
	} else if ( config.output == OF_PNG ) {
		rows = static_cast<png_bytepp>( calloc(oip->h, sizeof(png_bytep)) );
		if ( rows == nullptr ) { return FALSE; }
	} else {
		return FALSE;
	}
	width = oip->w;
	height = oip->h;
	dib_width = (width*3+3)&(~3);
	
	// 出力
	for (int i=0; i<oip->n; i++) {
		if ( oip->func_is_abort() ) { break; }
		oip->func_rest_time_disp(i, oip->n);
		try {
			const int j = i+config.offset;
			buff = std::vformat(config.fmt, std::make_format_args(j, name));
		} catch (std::format_error &err) {
			return finish_func_output(FALSE);
		}
		FILE *fp=_wfopen(Utf8ToUtf16(dir + buff + ext).c_str(), L"wb");
		if ( fp == nullptr ) { return finish_func_output(FALSE); }
		BOOL fail;
		if ( config.output == OF_JPEG ) {
			fail = put_jpeg_file(fp, static_cast<unsigned char *>( oip->func_get_video(i) ));
		} else if ( config.output == OF_PNG ) {
			fail = put_png_file(fp, static_cast<png_bytep>( oip->func_get_video(i) ));
		} else {
			fail = TRUE;
		}
		fclose(fp);
		if ( fail ) { return finish_func_output(FALSE); }
		oip->func_update_preview();
	}

	return finish_func_output(TRUE);
}

// JPEG出力 成功なら FALSE 失敗なら TRUE を返す
static BOOL
finish_put_jpeg_file(struct jpeg_compress_struct *jpegcp, const BOOL &ret)
{
	jpeg_destroy_compress(jpegcp);
	return ret;
}
static BOOL
put_jpeg_file(FILE *fp, const unsigned char *video)
{
	struct jpeg_compress_struct jpegc;
	ERROR_MGR myerr;
	const PIXEL_BGR *bgr_row;
	JSAMPLE *pixel;
	
	myerr.jerr.error_exit = error_exit;
	jpegc.err = jpeg_std_error(&myerr.jerr);
	if ( setjmp(myerr.jmpbuf) ) { return finish_put_jpeg_file(&jpegc, TRUE); }
	jpeg_create_compress(&jpegc);
	jpeg_stdio_dest(&jpegc, fp);
	jpegc.image_width = width;
	jpegc.image_height = height;
	jpegc.input_components = 3;
	jpegc.in_color_space = JCS_RGB;
	jpeg_set_defaults(&jpegc);
	jpeg_set_quality(&jpegc, config.jpeg_quality, TRUE);
	jpeg_start_compress(&jpegc, TRUE);
	for (int y=height-1; y>=0; y--) {
		bgr_row = reinterpret_cast<const PIXEL_BGR *>(video+y*dib_width);
		pixel = row;
		for (int x=0; x<width; x++) {
			*pixel++ = bgr_row[x].r;
			*pixel++ = bgr_row[x].g;
			*pixel++ = bgr_row[x].b;
		}
		jpeg_write_scanlines(&jpegc, &row, 1);
	}
	jpeg_finish_compress(&jpegc);

	return finish_put_jpeg_file(&jpegc, FALSE);
}

// PNG出力 成功なら FALSE 失敗なら TRUE を返す
static BOOL
finish_put_png_file(png_structp *pngp, png_infop *infop, const BOOL &ret)
{
	png_destroy_write_struct(pngp, infop);
	return ret;
}
static BOOL
put_png_file(FILE *fp, const png_bytep video)
{
	png_structp png = nullptr;
	png_infop info = nullptr;

	png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if ( png == nullptr ) { return finish_put_png_file(&png, &info, TRUE); }
	info = png_create_info_struct(png);
	if ( info == nullptr ) { return finish_put_png_file(&png, &info, TRUE); }
	if ( setjmp(png_jmpbuf(png)) ) { return finish_put_png_file(&png, &info, TRUE); }
	png_init_io(png, fp);
	png_set_IHDR(
		png, info, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT
	);
	for (int y=0; y<height; y++) {
		rows[height-y-1] = static_cast<png_bytep>(video+dib_width*y);
	}
	png_set_rows(png, info, rows);
	png_write_png(png, info, PNG_TRANSFORM_BGR, nullptr);

	return finish_put_png_file(&png, &info, FALSE);
}

// コンフィグ関係
LRESULT CALLBACK
func_config_proc(HWND hdlg, UINT umsg, WPARAM wparam, LPARAM lparam)
{
	static OUTPUT_FORMAT of_now=OF_END;
	if ( umsg == WM_INITDIALOG ) {
		SetDlgItemTextW(hdlg, IDC_FORMAT, Utf8ToUtf16(config.fmt).c_str());
		SetDlgItemTextW(hdlg, IDC_JPEGQ, std::format(L"{}", config.jpeg_quality).c_str());
		of_now = config.output;
		if ( of_now == OF_JPEG ) {
			SendMessage(GetDlgItem(hdlg, IDC_JPEG), BM_SETCHECK , TRUE , 0);
			EnableWindow(GetDlgItem(hdlg, IDC_JPEGQ), TRUE);
		} else if ( of_now == OF_PNG ) {
			SendMessage(GetDlgItem(hdlg, IDC_PNG), BM_SETCHECK , TRUE , 0);
			EnableWindow(GetDlgItem(hdlg, IDC_JPEGQ), FALSE);
		}
		SetDlgItemTextW(hdlg, IDC_OFFSET, std::format(L"{}", config.offset).c_str());
		return TRUE;
	} else if ( umsg==WM_COMMAND ) {
		WORD lwparam = LOWORD(wparam);
		if ( lwparam == IDCANCEL ) {
			EndDialog(hdlg, LOWORD(wparam));
		} else if ( lwparam == IDOK ) {
			config.output = of_now;
			std::wstring wstr(1023, L'\0');
			GetDlgItemTextW(hdlg, IDC_FORMAT, wstr.data(), wstr.size());
			config.fmt = Utf16ToUtf8(wstr);
			try {
				static_cast<void>(std::vformat(config.fmt, std::make_format_args(config.offset, "test")));
			} catch (std::format_error &err) {
				std::string str = std::format("ファイル名フォーマットに以下のエラーがあるため，デフォルト値「{}」に変更されました．\n{}", default_format, err.what());
				MessageBoxW(hdlg, Utf8ToUtf16(str).c_str(), L"フォーマット文字列エラー", MB_OK);
				config.fmt = default_format;
			}
			GetDlgItemTextW(hdlg, IDC_JPEGQ, wstr.data(), wstr.size());
			config.jpeg_quality = std::stoi(wstr);
			if ( config.jpeg_quality < 0 ) {
				config.jpeg_quality = 0;
			} else if ( 100 < config.jpeg_quality ) {
				config.jpeg_quality = 100;
			}
			GetDlgItemTextW(hdlg, IDC_OFFSET, wstr.data(), wstr.size());
			config.offset = std::stoi(wstr);
			EndDialog(hdlg, LOWORD(wparam));
		} else if ( lwparam == IDC_JPEG ) {
			of_now = OF_JPEG;
			EnableWindow(GetDlgItem(hdlg, IDC_JPEGQ), TRUE);
		} else if ( lwparam == IDC_PNG ) {
			of_now = OF_PNG;
			EnableWindow(GetDlgItem(hdlg, IDC_JPEGQ), FALSE);
		}
	}
	return FALSE;
}
BOOL
func_config(HWND hwnd, HINSTANCE dll_hinst)
{
	DialogBoxW(dll_hinst, L"CONFIG", hwnd, reinterpret_cast<DLGPROC>(func_config_proc));
	return TRUE;
}
int
func_config_get(void *data, int size)
{
	if ( data ) {
		CONFIG_NUM cn = {
			config.output,
			config.jpeg_quality,
			config.offset,
		};
		memcpy(data, &cn, sizeof(cn));
		memcpy(static_cast<char *>(data)+sizeof(cn), config.fmt.data(), config.fmt.size());
		return sizeof(cn)+config.fmt.size();
	}
	return 0;
}
int
func_config_set(void *data, int size)
{
	CONFIG_NUM cn;
	if ( size < static_cast<int>(sizeof(cn)) ) {
		return 0;
	}
	memcpy(&cn, data, sizeof(cn));
	config.output = cn.output;
	if ( cn.jpeg_quality < 0 ) {
		config.jpeg_quality = 0;
	} else if ( 100 < cn.jpeg_quality ) {
		config.jpeg_quality = 100;
	} else {
		config.jpeg_quality = cn.jpeg_quality;
	}
	config.offset = cn.offset;
	config.fmt.resize(size-sizeof(cn));
	memcpy(config.fmt.data(), static_cast<char *>(data)+sizeof(cn), size-sizeof(cn));
	try {
		static_cast<void>(std::vformat(config.fmt, std::make_format_args(config.offset, "test")));
	} catch (std::format_error &err) {
		config.fmt = default_format;
	}
	return size;
}
