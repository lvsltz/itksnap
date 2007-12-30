/*=========================================================================

  Program:   ITK-SNAP
  Module:    $RCSfile: RGBOverlayUILogic.cxx,v $
  Language:  C++
  Date:      $Date: 2007/12/30 04:05:17 $
  Version:   $Revision: 1.2 $
  Copyright (c) 2007 Paul A. Yushkevich
  
  This file is part of ITK-SNAP 

  ITK-SNAP is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  -----

  Copyright (c) 2003 Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notices for more information. 

=========================================================================*/
#include "RGBOverlayUILogic.h"

RGBOverlayUILogic
::RGBOverlayUILogic()
{
  m_EventSystem = EventSystemType::New();
}

void 
RGBOverlayUILogic
::DisplayWindow()
{
  // Show the window
  m_WinRGBOverlay->show();
}

unsigned char
RGBOverlayUILogic
::GetOpacity() const
{
  return (unsigned char) m_InRGBOverlayOpacity->value();
}

void
RGBOverlayUILogic
::SetOpacity(unsigned char opacity)
{
  m_InRGBOverlayOpacity->Fl_Valuator::value(opacity);
}

void 
RGBOverlayUILogic
::OnClose()
{
  m_WinRGBOverlay->hide();  
}

void 
RGBOverlayUILogic
::OnRGBOverlayOpacityChange()
{
  // Fire the event
  GetEventSystem()->InvokeEvent(
    RGBOverlayUILogic::OpacityUpdateEvent());
}

