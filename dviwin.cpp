//
// Class: dviWindow
//
// Previewer for TeX DVI files.
//

#include <stdlib.h>
#include <unistd.h>

#include <qpainter.h>
#include <qbitmap.h> 
#include <qkeycode.h>
#include <qpaintdevice.h>
#include <qfileinfo.h>

#include <kapp.h>
#include <kmessagebox.h>
#include <kdebug.h>
#include <klocale.h>

#include "dviwin.h"
#include "optiondialog.h"


//------ some definitions from xdvi ----------


#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

struct	WindowRec {
	Window		win;
	double		shrinkfactor;
	int		base_x, base_y;
	unsigned int	width, height;
	int	min_x, max_x, min_y, max_y;
};

extern	struct WindowRec mane, alt, currwin;

#include "c-openmx.h" // for OPEN_MAX

	float	_gamma;
	int	_pixels_per_inch;
	_Xconst char	*_paper;
	Pixel	_fore_Pixel;
	Pixel	_back_Pixel;
	Boolean	_postscript;
	Boolean	useGS;


extern char *           prog;
extern char *	        dvi_name;
extern FILE *		dvi_file;
extern int 		n_files_left;
extern int 		min_x;
extern int 		min_y;
extern int 		max_x;
extern int 		max_y;
extern int		offset_x, offset_y;
extern unsigned int	unshrunk_paper_w, unshrunk_paper_h;
extern unsigned int	unshrunk_page_w, unshrunk_page_h;
extern unsigned int	page_w, page_h;
extern int 		current_page;
extern int 		total_pages;
extern Display *	DISP;
extern Screen  *	SCRN;
Window mainwin;
int			useAlpha;

void 	draw_page(void);
extern "C" void 	kpse_set_progname(const char*);
extern Boolean check_dvi_file(void);
void 	reset_fonts();
void 	init_page();
void 	psp_destroy();
void 	psp_toggle();
void 	psp_interrupt();
extern "C" {
#undef PACKAGE // defined by both c-auto.h and config.h
#undef VERSION
#include <kpathsea/c-auto.h>
#include <kpathsea/paths.h>
#include <kpathsea/proginit.h>
#include <kpathsea/tex-file.h>
#include <kpathsea/tex-glyph.h>
}

//------ next the drawing functions called from C-code (dvi_draw.c) ----

QPainter *dcp;

extern  void qt_processEvents(void)
{
	qApp->processEvents();
}


//------ now comes the dviWindow class implementation ----------

dviWindow::dviWindow( int bdpi, int zoom, const char *mfm, const char *ppr, int mkpk, QWidget *parent, const char *name ) 
  : QWidget( parent, name )
{
  setBackgroundMode(NoBackground);

	ChangesPossible = 1;
	FontPath = QString::null;
	setFocusPolicy(QWidget::StrongFocus);
	setFocus();

	// initialize the dvi machinery

	setResolution( bdpi );
	setMakePK( mkpk );
	setMetafontMode( mfm );
	setPaper( ppr );

	DISP = x11Display();
	mainwin = handle();
	mane = currwin;
	SCRN = DefaultScreenOfDisplay(DISP);
	_fore_Pixel = BlackPixelOfScreen(SCRN);
	_back_Pixel = WhitePixelOfScreen(SCRN);
	useGS = 1;
	_postscript = 0;
	pixmap = NULL;

	double xres = ((double)(DisplayWidth(DISP,(int)DefaultScreen(DISP)) *25.4)/DisplayWidthMM(DISP,(int)DefaultScreen(DISP)) ); //@@@
	double s    = (basedpi * 100)/(xres*(double)zoom);
	mane.shrinkfactor = currwin.shrinkfactor = s;

	resize(0,0);
}

dviWindow::~dviWindow()
{
	psp_destroy();
}

void dviWindow::setShowPS( int flag )
{
	if ( _postscript == flag )
		return;
	_postscript = flag;
	psp_toggle();
	drawPage();
}

int dviWindow::showPS()
{
	return _postscript;
}

void dviWindow::setAntiAlias( int flag )
{
	if ( !useAlpha == !flag )
		return;
	useAlpha = flag;
	psp_destroy();
	drawPage();
}

int dviWindow::antiAlias()
{
	return useAlpha;
}

void dviWindow::setMakePK( int flag )
{
	if (!ChangesPossible)
		KMessageBox::sorry( this,
			i18n("The change in font generation will be effective\n"
			"only after you start kdvi again!") );
	makepk = flag;
}

int dviWindow::makePK()
{
	return makepk;	
}
	
void dviWindow::setFontPath( const char *s )
{
	if (!ChangesPossible)
		KMessageBox::sorry( this,
			i18n("The change in font path will be effective\n"
			"only after you start kdvi again!"));
	FontPath = s;
}

const char * dviWindow::fontPath()
{
	return FontPath;
}

void dviWindow::setMetafontMode( const char *mfm )
{
	if (!ChangesPossible)
		KMessageBox::sorry( this,
			i18n("The change in Metafont mode will be effective\n"
			"only after you start kdvi again!") );
	MetafontMode = mfm;
}

const char * dviWindow::metafontMode()
{
	return MetafontMode;
}

void dviWindow::setPaper( const char *paper )
{
	if ( !paper )
		return;
	paper_type = paper;
	_paper = paper_type;
	float w, h;
	if (!OptionDialog::paperSizes( paper, w, h ))
	{
		kDebugWarning( 4300, "Unknown paper type!");
		// A4 paper is used as default, if paper is unknown
		w = 21.0/2.54;
		h = 29.7/2.54;
	}
	unshrunk_paper_w = int( w * basedpi + 0.5 );
	unshrunk_paper_h = int( h * basedpi + 0.5 ); 
}

const char * dviWindow::paper()
{
	return paper_type;
}

void dviWindow::setResolution( int bdpi )
{
	if (!ChangesPossible)
		KMessageBox::sorry( this,
			i18n("The change in resolution will be effective\n"
			"only after you start kdvi again!") );
	basedpi = bdpi;
	_pixels_per_inch = bdpi;
	offset_x = offset_y = bdpi;
}

int dviWindow::resolution()
{
	return basedpi;
}

void dviWindow::setGamma( float gamma )
{
	if (!ChangesPossible)
	{
		KMessageBox::sorry( this,
			i18n("The change in gamma will be effective\n"
			"only after you start kdvi again!"), i18n( "OK" ) );
		return;	// Qt will kill us otherwise ??
	}
	_gamma = gamma;
}

float dviWindow::gamma()
{
	return _gamma;
}


void dviWindow::initDVI()
{
        prog = const_cast<char*>("kdvi");
	n_files_left = OPEN_MAX;
	kpse_set_progname ("xdvi");
	kpse_init_prog ("XDVI", basedpi, MetafontMode.data(), "cmr10");
	kpse_set_program_enabled(kpse_any_glyph_format,
				 makepk, kpse_src_client_cnf);
	kpse_format_info[kpse_pk_format].override_path
		= kpse_format_info[kpse_gf_format].override_path
		= kpse_format_info[kpse_any_glyph_format].override_path
		= kpse_format_info[kpse_tfm_format].override_path
		= FontPath.ascii();
	ChangesPossible = 0;
}

#include <setjmp.h>
extern	jmp_buf	dvi_env;	/* mechanism to communicate dvi file errors */
extern	char *dvi_oops_msg;
extern	QDateTime dvi_time;


//------ this function calls the dvi interpreter ----------

void dviWindow::drawPage()
{
  psp_interrupt();
  if (filename.isEmpty())	// must call setFile first
    return;
  if (!dvi_name) {			//  dvi file not initialized yet
    QApplication::setOverrideCursor( waitCursor );
    dvi_name = const_cast<char*>(filename.ascii());

    dvi_file = NULL;
    if (setjmp(dvi_env)) {	// dvi_oops called
      dvi_time.setTime_t(0); // force init_dvi_file
      QApplication::restoreOverrideCursor();
      KMessageBox::error( this,
			  i18n("What's this? DVI problem!\n")
			  + dvi_oops_msg);
      return;
    }
    check_dvi_file();
    QApplication::restoreOverrideCursor();
    gotoPage(1);
    changePageSize();
    return;
  }

  min_x = 0;
  min_y = 0;
  max_x = page_w;
  max_y = page_h;

  if ( !pixmap )
    return;

  if ( !pixmap->paintingActive() ) {
    QPainter paint;
    paint.begin( pixmap );
    QApplication::setOverrideCursor( waitCursor );
    dcp = &paint;
    if (setjmp(dvi_env)) {	// dvi_oops called
      dvi_time.setTime_t(0); // force init_dvi_file
      QApplication::restoreOverrideCursor();
      paint.end();
      KMessageBox::error( this,
			  i18n("What's this? DVI problem!\n") 
			  + dvi_oops_msg);
      return;
    } else {
      check_dvi_file();
      pixmap->fill( white );
      draw_page();
    }
    QApplication::restoreOverrideCursor();
    paint.end();
  }
  resize(pixmap->width(), pixmap->height());
  repaint();
}


bool dviWindow::correctDVI()
{
  QFile f(filename);
  if (!f.open(IO_ReadOnly))
    return FALSE;
  int n = f.size();
  if ( n < 134 )	// Too short for a dvi file
    return FALSE;
  f.at( n-4 );
  char test[4];
  unsigned char trailer[4] = { 0xdf,0xdf,0xdf,0xdf };
  if ( f.readBlock( test, 4 )<4 || strncmp( test, (char *) trailer, 4 ) )
    return FALSE;
  // We suppose now that the dvi file is complete	and OK
  return TRUE;
}


void dviWindow::changePageSize()
{
  if ( pixmap && pixmap->paintingActive() )
    return;
  psp_destroy();
  int old_width = 0;
  if (pixmap) {
    old_width = pixmap->width();
    delete pixmap;
  }
  pixmap = new QPixmap( (int)page_w, (int)page_h );
  pixmap->fill( white );

  resize( page_w, page_h );
  currwin.win = mane.win = pixmap->handle();
  drawPage();
}

//------ setup the dvi interpreter (should do more here ?) ----------

void dviWindow::setFile( const char *fname )
{
        if (ChangesPossible){
            initDVI();
        }
        filename = fname;
        dvi_name = 0;
        drawPage();
}


//------ handling pages ----------


void dviWindow::gotoPage(int new_page)
{
  if (new_page<1)
    new_page = 1;
  if (new_page>total_pages)
    new_page = total_pages;
  if (new_page-1==current_page)
    return;
  current_page = new_page-1;
  drawPage();
}


int dviWindow::totalPages()
{
	return total_pages;
}


void dviWindow::setZoom(int zoom)
{
  if ((zoom < 5) || (zoom > 500))
    zoom = 100;

  double xres = ((double)(DisplayWidth(DISP,(int)DefaultScreen(DISP)) *25.4)/DisplayWidthMM(DISP,(int)DefaultScreen(DISP)) ); //@@@
  double s    = (basedpi * 100)/(xres*(double)zoom);
  mane.shrinkfactor = currwin.shrinkfactor = s;
  init_page();
  reset_fonts();
  changePageSize();
}


void dviWindow::paintEvent(QPaintEvent *ev)
{
  if (pixmap)
    {
      QPainter p(this);
      p.drawPixmap(QPoint(0, 0), *pixmap);
    }
}
