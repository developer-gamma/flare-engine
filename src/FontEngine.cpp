/*
Copyright © 2011-2012 Clint Bellanger and Thane Brimhall
Copyright © 2013 Kurt Rinnert
Copyright © 2014 Henrik Andersson
Copyright © 2015 Igor Paliychuk
Copyright © 2012-2016 Justin Jacobs

This file is part of FLARE.

FLARE is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

FLARE is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
FLARE.  If not, see http://www.gnu.org/licenses/
*/

/*
 * class FontEngine
 */

#include "FontEngine.h"
#include "UtilsParsing.h"

FontStyle::FontStyle() : name(""), path(""), ptsize(0), blend(true), line_height(0), font_height(0) {
}

FontEngine::FontEngine() : cursor_y(0) {
}

Color FontEngine::getColor(const std::string& _color) {
	std::map<std::string,Color>::iterator it,end;
	for (it=color_map.begin(), end=color_map.end(); it!=end; ++it) {
		if (_color.compare(it->first) == 0) return it->second;
	}
	// If all else fails, return white;
	return FONT_WHITE;
}

/**
 * Using the given wrap width, calculate the width and height necessary to display this text
 */
Point FontEngine::calc_size(const std::string& text_with_newlines, int width) {
	char newline = 10;

	std::string text = text_with_newlines;

	// if this contains newlines, recurse
	size_t check_newline = text.find_first_of(newline);
	if (check_newline != std::string::npos) {
		Point p1 = calc_size(text.substr(0, check_newline), width);
		Point p2 = calc_size(text.substr(check_newline+1, text.length()), width);
		Point p3;

		if (p1.x > p2.x) p3.x = p1.x;
		else p3.x = p2.x;

		p3.y = p1.y + p2.y;
		return p3;
	}

	int height = 0;
	int max_width = 0;

	std::string next_word;
	std::stringstream builder;
	std::stringstream builder_prev;
	char space = 32;
	size_t cursor = 0;
	std::string fulltext = text + " ";
	std::string long_token;

	builder.str("");
	builder_prev.str("");

	next_word = getNextToken(fulltext, cursor, space);

	while(cursor != std::string::npos) {
		builder << next_word;

		if (calc_width(builder.str()) > width) {

			// this word can't fit on this line, so word wrap
			if (!builder_prev.str().empty()) {
				height += getLineHeight();
				if (calc_width(builder_prev.str()) > max_width) {
					max_width = calc_width(builder_prev.str());
				}
			}

			builder_prev.str("");
			builder.str("");

			long_token = popTokenByWidth(next_word, width);

			if (!long_token.empty()) {
				while (!long_token.empty()) {
					if (calc_width(next_word) > max_width) {
						max_width = calc_width(next_word);
					}
					height += getLineHeight();

					next_word = long_token;
					long_token = popTokenByWidth(next_word, width);
				}
			}

			builder << next_word << " ";
			builder_prev.str(builder.str());
		}
		else {
			builder <<  " ";
			builder_prev.str(builder.str());
		}

		next_word = getNextToken(fulltext, cursor, space); // get next word
	}

	builder.str(trim(builder.str())); //removes whitespace that shouldn't be included in the size
	if (!builder.str().empty())
		height += getLineHeight();
	if (calc_width(builder.str()) > max_width) max_width = calc_width(builder.str());

	// handle blank lines
	if (text_with_newlines == " ")
		height += getLineHeight();

	Point size;
	size.x = max_width;
	size.y = height;
	return size;
}

Rect FontEngine::position(const std::string& text, int x, int y, int justify) {

	Rect dest_rect;
	// calculate actual starting x,y based on justify
	if (justify == JUSTIFY_LEFT) {
		dest_rect.x = x;
		dest_rect.y = y;
	}
	else if (justify == JUSTIFY_RIGHT) {
		dest_rect.x = x - calc_width(text);
		dest_rect.y = y;
	}
	else if (justify == JUSTIFY_CENTER) {
		dest_rect.x = x - calc_width(text)/2;
		dest_rect.y = y;
	}
	else {
		logError("FontEngine::position() given unhandled 'justify=%d', assuming left",justify);
		dest_rect.x = x;
		dest_rect.y = y;
	}
	return dest_rect;
}

/**
 * Word wrap to width
 */
void FontEngine::render(const std::string& text, int x, int y, int justify, Image *target, int width, const Color& color) {
	if (width == 0) {
		// a width of 0 means we won't try to wrap text
		renderInternal(text, x, y, justify, target, color);
		return;
	}

	std::string fulltext = text + " ";
	cursor_y = y;
	std::string next_word;
	std::stringstream builder;
	std::stringstream builder_prev;
	char space = 32;
	size_t cursor = 0;
	std::string long_token;

	builder.str("");
	builder_prev.str("");

	next_word = getNextToken(fulltext, cursor, space);

	while(cursor != std::string::npos) {

		builder << next_word;

		if (calc_width(builder.str()) > width) {
			if (!builder_prev.str().empty()) {
				renderInternal(builder_prev.str(), x, cursor_y, justify, target, color);
				cursor_y += getLineHeight();
			}
			builder_prev.str("");
			builder.str("");

			long_token = popTokenByWidth(next_word, width);

			if (!long_token.empty()) {
				while (!long_token.empty()) {
					renderInternal(next_word, x, cursor_y, justify, target, color);
					cursor_y += getLineHeight();

					next_word = long_token;
					long_token = popTokenByWidth(next_word, width);
				}
			}

			builder << next_word << " ";
			builder_prev.str(builder.str());
		}
		else {
			builder << " ";
			builder_prev.str(builder.str());
		}

		next_word = getNextToken(fulltext, cursor, space); // next word
	}

	renderInternal(builder.str(), x, cursor_y, justify, target, color);
	cursor_y += getLineHeight();

}

void FontEngine::renderShadowed(const std::string& text, int x, int y, int justify, Image *target, int width, const Color& color) {
	render(text, x+1, y+1, justify, target, width, FONT_BLACK);
	render(text, x, y, justify, target, width, color);
}

/*
 * Fits a string, "text", to a pixel "width".
 * The original string is mutated to fit within the width.
 * Returns a string that is the remainder of the original string that could not fit in the width.
 */
std::string FontEngine::popTokenByWidth(std::string& text, int width) {
	size_t new_length = 0;

	for (size_t i = 0; i <= text.length(); ++i) {
		if (i < text.length() && (text[i] & 0xc0) == 0x80)
			continue;

		if (calc_width(text.substr(0, i)) > width)
			break;

		new_length = i;
	}

	if (new_length > 0) {
		std::string ret = text.substr(new_length, text.length());
		text = text.substr(0, new_length);
		return ret;
	}
	else {
		return text;
	}
}
