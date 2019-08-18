#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <png.h>
#include "output.h"
#include "image_output_ks.h"
#include "version.h"

typedef struct {
	unsigned char b;
	unsigned char g;
	unsigned char r;
} PIXEL_BGR;

typedef struct {
	struct jpeg_error_mgr jerr;
	jmp_buf jmpbuf;
} my_error_mgr;
static void my_error_exit(j_common_ptr cinfo) {
	longjmp((reinterpret_cast<my_error_mgr *>(cinfo->err))->jmpbuf, 1);
}

typedef enum {
	OF_JPEG,
	OF_PNG,
	OF_END
} OUTPUT_FORMAT;
typedef enum {
	FFS_NAME_NUMBER,
	FFS_NUMBER,
	FFS_NUMBER_NAME,
	FFS_END
} FILE_FORMAT_STYLE;
typedef struct {
	OUTPUT_FORMAT output;
	TCHAR fmt[256];
	FILE_FORMAT_STYLE ffs;
	int jpeg_quality;
	int offset;
} CONFIG;
static CONFIG config = {OF_JPEG, "%s_%04d", FFS_NAME_NUMBER, 75, 0};

static char pname[] = "連番画像出力";
static char filefilter[] = "JPEG File (*.jpg)\0*.jpg\0PNG File (*.png)\0*.png\0All File (*.*)\0*.*\0";
static char information[] = "連番画像出力 " VERSION " by KAZOON";
OUTPUT_PLUGIN_TABLE output_plugin_table = {
	0,
	pname,
	filefilter,
	information,
	NULL, // func_init
	NULL, // func_exit
	func_output,
	func_config,
	func_config_get,
	func_config_set,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 // reserve
};

EXTERN_C OUTPUT_PLUGIN_TABLE __declspec(dllexport) * __stdcall GetOutputPluginTable(void) {
	return &output_plugin_table;
}

static void prepare_path(const LPSTR savefile, TCHAR *path, TCHAR *name, TCHAR *ext, TCHAR **startp);
static BOOL put_jpeg_file(FILE *fp, unsigned char *video);
static BOOL put_png_file(FILE *fp, png_bytep video);

static JSAMPROW row=NULL;
static png_bytepp rows=NULL;
static int width, height, dib_width;

static BOOL finish_func_output(BOOL ret) {
	free(row);
	row = NULL;
	free(rows);
	rows=NULL;
	return ret;
}
BOOL func_output(OUTPUT_INFO *oip) {
	TCHAR path[MAX_PATH], name[MAX_PATH], ext[MAX_PATH], *spf_start, buff[MAX_PATH];
	prepare_path(oip->savefile, path, name, ext, &spf_start);
	
	if ( config.output == OF_JPEG ) {
		row = static_cast<JSAMPROW>( calloc(oip->w*3, sizeof(JSAMPLE)) );
		if (row==NULL) { return FALSE; }
	} else if ( config.output == OF_PNG ) {
		rows = static_cast<png_bytepp>( calloc(oip->h, sizeof(png_bytep)) );
		if (rows==NULL) { return FALSE; }
	} else {
		return FALSE;
	}
	width = oip->w;
	height = oip->h;
	dib_width = (width*3+3)&(~3);
	
	// 出力
	for (int i=0; i<oip->n; i++) {
		if (oip->func_is_abort()) { break; }
		oip->func_rest_time_disp(i, oip->n);
		if (config.ffs==FFS_NAME_NUMBER) {
			wsprintf(buff, config.fmt, name, i+config.offset);
		} else if (config.ffs==FFS_NUMBER) {
			wsprintf(buff, config.fmt, i+config.offset);
		} else if (config.ffs==FFS_NUMBER_NAME) {
			wsprintf(buff, config.fmt, i+config.offset, name);
		} else {
			return finish_func_output(FALSE);
		}
		wsprintf(spf_start, "%s%s", buff, ext);
		FILE *fp=fopen(path, "wb");
		if (fp==NULL) { return finish_func_output(FALSE); }
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

// 連番ファイル名の準備
static void prepare_path(const LPSTR savefile, TCHAR *path, TCHAR *name, TCHAR *ext, TCHAR **startp) {
	lstrcpy(path, savefile);
	BOOL flg = TRUE;
	*startp = path;
	TCHAR *ext_start=path;
	for (TCHAR *p=path; *p; p++) {
		if (*p == '\\') {
			*startp = p+1;
		}
		if (*p == '.') {
			ext_start = p;
			flg = FALSE;
		} else if (flg) {
			ext_start = p+1;
		}
	}
	lstrcpy(ext, ext_start);
	*ext_start = '\0';
	lstrcpy(name, *startp);
}

// JPEG出力 成功なら FALSE 失敗なら TRUE を返す
static BOOL finish_put_jpeg_file(struct jpeg_compress_struct *jpegcp, BOOL ret) {
	jpeg_destroy_compress(jpegcp);
	return ret;
}
static BOOL put_jpeg_file(FILE *fp, unsigned char *video) {
	struct jpeg_compress_struct jpegc;
	my_error_mgr myerr;
	PIXEL_BGR *bgr_row;
	JSAMPLE *pixel;
	
	myerr.jerr.error_exit = my_error_exit;
	jpegc.err = jpeg_std_error(&myerr.jerr);
	if (setjmp(myerr.jmpbuf)) { return finish_put_jpeg_file(&jpegc, TRUE); }
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
		bgr_row = reinterpret_cast<PIXEL_BGR *>(video+y*dib_width);
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
static BOOL finish_put_png_file(png_structp *pngp, png_infop *infop, BOOL ret) {
	png_destroy_write_struct(pngp, infop);
	return ret;
}
static BOOL put_png_file(FILE *fp, png_bytep video) {
	png_structp png = NULL;
	png_infop info = NULL;

	png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png == NULL) { return finish_put_png_file(&png, &info, TRUE); }
	info = png_create_info_struct(png);
	if (info == NULL) { return finish_put_png_file(&png, &info, TRUE); }
	if (setjmp(png_jmpbuf(png))) { return finish_put_png_file(&png, &info, TRUE); }
	png_init_io(png, fp);
	png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	for (int y=0; y<height; y++) {
		rows[height-y-1] = static_cast<png_bytep>(video+dib_width*y);
	}
	png_set_rows(png, info, rows);
	png_write_png(png, info, PNG_TRANSFORM_BGR, NULL);

	return finish_put_png_file(&png, &info, FALSE);
}

// コンフィグ関係

LRESULT CALLBACK func_config_proc(HWND hdlg, UINT umsg, WPARAM wparam, LPARAM lparam) {
	TCHAR buf[16];
	static OUTPUT_FORMAT of_now=OF_END;
	if (umsg == WM_INITDIALOG) {
		SetDlgItemText(hdlg, IDC_FORMAT, config.fmt);
		wsprintf(buf, "%d", config.jpeg_quality);
		SetDlgItemText(hdlg, IDC_JPEGQ, buf);
		of_now = config.output;
		if ( of_now == OF_JPEG ) {
			SendMessage(GetDlgItem(hdlg, IDC_JPEG), BM_SETCHECK , TRUE , 0);
			EnableWindow(GetDlgItem(hdlg, IDC_JPEGQ), TRUE);
		} else if ( of_now == OF_PNG ) {
			SendMessage(GetDlgItem(hdlg, IDC_PNG), BM_SETCHECK , TRUE , 0);
			EnableWindow(GetDlgItem(hdlg, IDC_JPEGQ), FALSE);
		}
		wsprintf(buf, "%d", config.offset);
		SetDlgItemText(hdlg, IDC_OFFSET, buf);
		return TRUE;
	} else if (umsg==WM_COMMAND) {
		WORD lwparam = LOWORD(wparam);
		if (lwparam == IDCANCEL ) {
			EndDialog(hdlg, LOWORD(wparam));
		} else if (lwparam == IDOK) {
			config.output = of_now;
			GetDlgItemText(hdlg, IDC_FORMAT, config.fmt, sizeof(config.fmt)-1);
			BOOL s_found=FALSE, d_found=FALSE;
			config.ffs = FFS_END;
			for (TCHAR *p=config.fmt; *p; p++) {
				if (*p=='%') {
					if (*(p+1)=='%') {
						p++;
					} else if (*(p+1)=='s') {
						if (d_found) {
							config.ffs = FFS_NUMBER_NAME;
							break;
						} else {
							s_found = TRUE;
						}
					} else {
						if (s_found) {
							config.ffs = FFS_NAME_NUMBER;
							break;
						} else {
							d_found = TRUE;
							config.ffs = FFS_NUMBER;
						}
					}
				}
			}
			GetDlgItemText(hdlg, IDC_JPEGQ, buf, sizeof(buf)-1);
			config.jpeg_quality = atoi(buf);
			GetDlgItemText(hdlg, IDC_OFFSET, buf, sizeof(buf)-1);
			config.offset = atoi(buf);
			EndDialog(hdlg, LOWORD(wparam));
		} else if (lwparam == IDC_JPEG) {
			of_now = OF_JPEG;
			EnableWindow(GetDlgItem(hdlg, IDC_JPEGQ), TRUE);
		} else if (lwparam == IDC_PNG) {
			of_now = OF_PNG;
			EnableWindow(GetDlgItem(hdlg, IDC_JPEGQ), FALSE);
		}
	}
	return FALSE;
}
BOOL func_config(HWND hwnd, HINSTANCE dll_hinst) {
	DialogBox(dll_hinst, "CONFIG", hwnd, (DLGPROC)func_config_proc);
	return TRUE;
}
int func_config_get(void *data, int size) {
	if (data) {
		memcpy(data, &config, sizeof(config));
		return sizeof(config);
	}
	return 0;
}
int func_config_set(void *data, int size) {
	if (size != sizeof(config)) {
		return 0;
	}
	memcpy(&config, data, size);
	return size;
}
