/***************************************************************************
 *   Copyright (C) 2005 by Albert Astals Cid <tsdgeos@terra.es>            *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <qpainter.h>
#include <qpixmap.h>
#include <qimage.h>
#include <kimageeffect.h>
#include <kprinter.h>

#include "core/page.h"
#include "generator_kimgio.h"

OKULAR_EXPORT_PLUGIN(KIMGIOGenerator)

KIMGIOGenerator::KIMGIOGenerator( KPDFDocument * document ) : Generator( document )
{
}

KIMGIOGenerator::~KIMGIOGenerator()
{
}

bool KIMGIOGenerator::loadDocument( const QString & fileName, QVector<KPDFPage*> & pagesVector )
{
    m_pix = new QPixmap(fileName);

    pagesVector.resize( 1 );

    KPDFPage * page = new KPDFPage( 0, m_pix->width(), m_pix->height(), 0 );
    pagesVector[0] = page;

    return true;
}

bool KIMGIOGenerator::closeDocument()
{
    delete m_pix;
    m_pix = 0;

    return true;
}

bool KIMGIOGenerator::canGeneratePixmap( bool /* async */ )
{
    return true;
}

void KIMGIOGenerator::generatePixmap( PixmapRequest * request )
{
    // perform a smooth scaled generation
    QImage smoothImage = m_pix->toImage().scaled( request->width, request->height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
    // rotate, if necessary
    int rotation = m_document->rotation();
    QImage finalImage = rotation > 0
        ? KImageEffect::rotate( smoothImage, (KImageEffect::RotateDirection)( rotation - 1 ) )
        : smoothImage;
    QPixmap * p = new QPixmap();
    *p = QPixmap::fromImage( finalImage );
    request->page->setPixmap(request->id, p);

    // signal that the request has been accomplished
    signalRequestDone(request);
}

void KIMGIOGenerator::setOrientation( QVector<KPDFPage*> & pagesVector, int orientation )
{
   int w = m_pix->width();
   int h = m_pix->height();
   if ( orientation % 2 == 1 )
       qSwap( w, h );

    delete pagesVector[0];

    KPDFPage * page = new KPDFPage( 0, w, h, orientation );
    pagesVector[0] = page;
}

bool KIMGIOGenerator::print( KPrinter& printer )
{
    QPainter p(&printer);
    p.drawPixmap(0, 0, *m_pix);
    return true;
}

#include "generator_kimgio.moc"

