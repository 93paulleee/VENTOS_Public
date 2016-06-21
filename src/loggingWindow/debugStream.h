/****************************************************************************/
/// @file    debugStream.h
/// @author  Mani Amoozadeh <maniam@ucdavis.edu>
/// @author  second author name
/// @date    May 2016
///
/****************************************************************************/
// VENTOS, Vehicular Network Open Simulator; see http:?
// Copyright (C) 2013-2015
/****************************************************************************/
//
// This file is part of VENTOS.
// VENTOS is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#ifndef DEBUGSTREAM_H
#define DEBUGSTREAM_H

#include <cassert>
#include <streambuf>
#include <vector>
#include <gtkmm/main.h>

namespace VENTOS {

class debugStream : public std::streambuf
{
public:

    explicit debugStream(Gtk::TextView *tw, std::size_t buff_sz = 512) : m_TextView(tw), buffer_(buff_sz + 1)
    {
        char *base = &buffer_.front();
        setp(base, base + buffer_.size() - 1); // -1 to make overflow() easier
    }

    debugStream(const debugStream &);
    debugStream &operator= (const debugStream &);

protected:

    // overflow is called whenever pptr() == epptr()
    virtual int_type overflow(int_type ch)
    {
        if (ch != traits_type::eof())
        {
            // making sure 'pptr' have not passed 'epptr'
            assert(std::less_equal<char *>()(pptr(), epptr()));

            *pptr() = ch;
            pbump(1);  // advancing the write position

            std::ptrdiff_t n = pptr() - pbase();
            pbump(-n);

            // inserting the first n characters pointed by pbase() into the sink_
            std::ostringstream sink_;
            sink_.write(pbase(), n);

            updateTextView(sink_);

            return ch;
        }

        return traits_type::eof();
    }

    // write the current buffered data to the target, even when the buffer isn't full.
    // This could happen when the std::flush manipulator is used on the stream
    virtual int sync()
    {
        std::ptrdiff_t n = pptr() - pbase();
        pbump(-n);

        // inserting the first n characters pointed by pbase() into the sink_
        std::ostringstream sink_;
        sink_.write(pbase(), n);

        updateTextView(sink_);

        return 0;
    }

private:

    void updateTextView(std::ostringstream & sink_)
    {
        auto textBuffer = m_TextView->get_buffer();
        auto iter = textBuffer->end();
        textBuffer->insert(iter, sink_.str());

        // scroll the last inserted line into view
        auto iter2 = textBuffer->end();
        iter2.set_line_offset(0);  // Beginning of last line
        auto mark = textBuffer->get_mark("last_line");
        textBuffer->move_mark(mark, iter2);
        m_TextView->scroll_to(mark);
    }

    void addTextFormatting()
    {


    }

private:
    Gtk::TextView *m_TextView;
    std::vector<char> buffer_;
};

}

#endif
