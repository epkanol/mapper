/*
 *    Copyright 2012, 2013 Thomas Schöps
 *
 *    This file is part of OpenOrienteering.
 *
 *    OpenOrienteering is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    OpenOrienteering is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with OpenOrienteering.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _OPENORIENTEERING_OBJECT_TEXT_H_
#define _OPENORIENTEERING_OBJECT_TEXT_H_

#include <cassert>
#include <vector>

#include <QString>
#include <QFontMetricsF>
#include <QPointF>
#include <QTransform>

#include "object.h"
#include "map_coord.h"

class Symbol;

/** TextObjectPartInfo contains layout information for a continuous sequence of printable characters
 *  in a longer text.
 */
struct TextObjectPartInfo
{
	QString part_text;		/// The sequence of printable characters which makes up this part
	int start_index;		/// The index of the part's first character in the original string
	int end_index;			/// The index of the part's last character in the original string
	double part_x;			/// The left endpoint of the baseline of this part in text coordinates
	double width;			/// The width of the rendered part in text coordinates
	
	/** Create a new TextObjectPartInfo.
	 *  All information must be supplied as parameters.
	 *  (Assumes that the values have been precalculated in a layout algorithm.)
	 */
	inline TextObjectPartInfo(const QString& text, int start_index, int end_index, double part_x, double width, const QFontMetricsF& metrics)
	 : part_text(text), start_index(start_index), end_index(end_index), part_x(part_x), width(width), metrics(metrics) {}
	
	/** Get the horizontal position of a particular character in a part.
	 *  @param index the index of the character in the original string
	 *  @return      the character's horizontal position in text coordinates
	 */
	inline double getX(int index) const { return part_x + metrics.width(part_text.left(index - start_index)); }
	
	/** Find the index of the character corresponding to a particular position.
	 *  @param pos_x the position for which the index is requested
	 *  @return      the character's index in the original string
	 */
	int getIndex(double pos_x) const;
	
protected:
	 QFontMetricsF metrics;	// The metrics of the font that is used to render the part
};



/** TextObjectLineInfo contains layout information for a single line
 * in a longer text. A line is a sequence of different parts.
 */
struct TextObjectLineInfo
{
	/** A sequence container of TextObjectPartInfo objects
	*/
	typedef std::vector<TextObjectPartInfo> PartInfoContainer;

	int start_index;		/// The index of the part's first character in the original string
	int end_index;			/// The index of the part's last character in the original string
	bool paragraph_end;		/// Is this line the end of a paragraph?
	double line_x;			/// The left endpoint of the baseline of this line in text coordinates
	double line_y;			/// The vertical position of the baseline of this line in text coordinates
	double width;			/// The total width of the text in this line
	double ascent;			/// The height of the rendered text above the baseline 
	double descent;			/// The height of the rendered text below the baseline 
	PartInfoContainer part_infos; /// The sequence of parts which make up this line
	
	/** Create a new TextObjectLineInfo.
	 *  All information must be supplied as parameters.
	 *  (Assumes that the values have been precalculated in a layout algorithm.)
	 */
	inline TextObjectLineInfo(int start_index, int end_index, bool paragraph_end, double line_x, double line_y, double width, double ascent, double descent, PartInfoContainer& part_infos)
	 : start_index(start_index), end_index(end_index), paragraph_end(paragraph_end), line_x(line_x), line_y(line_y), width(width), ascent(ascent), descent(descent), part_infos(part_infos) {}
	
	/** Get the horizontal position of a particular character in a line.
	 *  @param pos the index of the character in the original string
	 *  @return    the character's horizontal position in text coordinates
	 */
	double getX(int pos) const;
	
	/** Find the index of the character corresponding to a particular position.
	 *  @param pos_x the position for which the index is requested
	 *  @return      the character's index in the original string
	 */
	int getIndex(double pos_x) const;
};

/** A text object.
 * 
 *  A text object is an instance of a text symbol. 
 *  Its position may be specified by a single coordinate (the anchor point) 
 *  or by two coordinates (word wrap box: 
 *  first coordinate specifies the coordinate of the midpoint,
 *  second coordinates specifies the width and height).
 * 
 * TODO: the way of defining word wrap boxes is inconvenient, as the second
 * coordinate does not specify a real coordinate in this case, but is misused
 * as extent. Change this?
 */
class TextObject : public Object
{
public:
	enum HorizontalAlignment
	{
		AlignLeft = 0,
		AlignHCenter = 1,
		AlignRight = 2
	};
	
	enum VerticalAlignment
	{
		AlignBaseline = 0,
		AlignTop = 1,
		AlignVCenter = 2,
		AlignBottom = 3
	};
	
	/** A sequence container of TextObjectLineInfo objects
	*/
	typedef std::vector<TextObjectLineInfo> LineInfoContainer;

	/** Construct a new text object.
	 *  If a symbol is specified, it must be a text symbol.
	 *  @param symbol the text symbol (optional)
	 */
	TextObject(Symbol* symbol = NULL);
	
	/** Create a duplicate of the object.
	 *  @return a new object with same text, symbol and formatting.
	 */
	virtual Object* duplicate();
	
    virtual Object& operator=(const Object& other);
	
	
	/** Returns true if the text object has a single anchor, false if it has as word wrap box
	 */
	inline bool hasSingleAnchor() const {return coords.size() == 1;}
	
	/** Sets the position of the anchor point to (x,y). 
	 *  This will drop an existing word wrap box.
	 */
	void setAnchorPosition(qint64 x, qint64 y);
	
	/** Sets the position of the anchor point to coord. 
	 *  This will drop an existing word wrap box.
	 */
	void setAnchorPosition(MapCoordF coord);
	
	/* Not used:
	void getAnchorPosition(qint64& x, qint64& y) const;	// or midpoint if a box is used
	*/
	
	/** Returns the coordinates of the anchor point or midpoint */
	MapCoordF getAnchorCoordF() const;
	
	/** Set position and size. 
	 *  The midpoint is set to (mid_x, mid_y), the size is specifed by the parameters
	 *  width and heigt.
	 */
	void setBox(qint64 mid_x, qint64 mid_y, double width, double height);
	
	/** Returns the width of the word wrap box.
	 *  The text object must have a specified size.
	 */
	inline double getBoxWidth() const {assert(!hasSingleAnchor()); return coords[1].xd();}
	
	/** Returns the height of the word wrap box.
	 *  The text object must have a specified size.
	 */
	inline double getBoxHeight() const {assert(!hasSingleAnchor()); return coords[1].yd();}
	
	
	/** Sets the text of the object.
	 */
	void setText(const QString& text);
	
	/** Returns the text of the object.
	 */
	inline const QString& getText() const {return text;}
	
	/** Sets the horizontal alignment of the text.
	 */ 
	void setHorizontalAlignment(HorizontalAlignment h_align);
	
	/** Returns the horizontal alignment of the text.
	 */ 
	inline HorizontalAlignment getHorizontalAlignment() const {return h_align;}
	
	/** Sets the vertical alignment of the text.
	 */ 
	void setVerticalAlignment(VerticalAlignment v_align);
	
	/** Returns the vertical alignment of the text.
	 */ 
	inline VerticalAlignment getVerticalAlignment() const {return v_align;}
		
	/** Sets the rotation of the text.
	 *  The rotation is measured in radians. The center of rotation is the anchor point.
	 */ 
	void setRotation(float new_rotation);
	
	/** Returns the rotation of the text.
	 *  The rotation is measured in radians. The center of rotation is the anchor point.
	 */ 
	inline float getRotation() const {return rotation;}
	
	
	/** Returns a QTransform from text coordinates to map coordinates.
	 */
	QTransform calcTextToMapTransform() const;
	
	/** Returns a QTransform from map coordinates to text coordinates.
	 */
	QTransform calcMapToTextTransform() const;
	
	
	/** Return the number of rendered lines.
	 * For a text object with a word wrap box, the number of rendered lines
	 * may be higher than the number of explicit line breaks in the original text.
	 */
	inline int getNumLines() const {return (int)line_infos.size();}
	
	/** Returns the layout information about a particular line.
	 */
	inline TextObjectLineInfo* getLineInfo(int i) {return &line_infos[i];}
	
	/** Return the index of the character or the line number corresponding to a particular map coordinate.
	 *  Returns -1 if the coordinate is not at a text position. 
	 *  If find_line_only is true, the line number is returned, otherwise the index of the character.
	 */
	int calcTextPositionAt(MapCoordF coord, bool find_line_only);
	
	/** Return the index of the character or the line number corresponding to a particular text coordinate.
	 *  Returns -1 if the coordinate is not at a text position.
	 *  If find_line_only is true, the line number is returned, otherwise the index of the character.
	 */
	int calcTextPositionAt(QPointF coord, bool find_line_only);

	/** Returns the line number for a particular index in the text.
	 */
	int findLineForIndex(int index);
	
	/** Returns the line layout information for particular index.
	 */
	const TextObjectLineInfo& findLineInfoForIndex(int index);
	
	/** Prepare the text layout information.
	 */
	void prepareLineInfos();
	
private:
	QString text;
	HorizontalAlignment h_align;
	VerticalAlignment v_align;
	float rotation;	// 0 to 2*M_PI
	
	/** Information about the text layout.
	 */
	LineInfoContainer line_infos;
};

#endif