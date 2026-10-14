//**************************************************************************
//*  Curve Applet by Michael Heinrichs (mike_heinrichs@hotmail.com)
//*
//*  This applet allows the user to specify control points to be used for
//*  plotting curves.  Three curve types are supported: Hermite, Bezier,
//*  and B-Spline.  The user can add, delete and move control points.
//*  
//*  This applet was inspired by the DrawTest demo applet included in the
//*  1.0 beta Java Developer's Kit from Sun Microsystems.
//*  If you are interested in the algorithms used to display the curves,
//*  Please see  the book: "Computer Graphics.  Principles and Practice" by
//*  Foley, VanDam, Feiner, and Hughes.
//*
//*  Permission to use, copy, modify, and distribute this software
//*  for NON-COMMERCIAL purposes without fee is hereby granted.
//*  If you modify and/or extend this program and distribute the
//*  result, please inform me (at the above email address), and
//*  mention my name and email address in the resulting program.
//**************************************************************************

import java.awt.*;
import java.applet.*;

import java.util.Vector;

public class Curve extends Applet {
	public void init() {
		setLayout(new BorderLayout());
		CurvePanel dp = new CurvePanel();
		add("Center", dp);
		add("South",new CurveControls(dp));
		add("North",new CurveControls2(dp));
	}

	public boolean handleEvent(Event e) {
		switch (e.id) {
			case Event.WINDOW_DESTROY:
				System.exit(0);
				return true;
			default:
				return false;
		}
	}

	public static void main(String args[]) {
		Frame f = new Frame("Curve");
		f.resize(500,400);
		Curve curve = new Curve();
		curve.init();

		f.add("Center", curve);
		f.show();
	}
}

class ControlPoint extends Object {
public int x;
public int y;
public static final int PT_SIZE = 4;

	public ControlPoint(int a, int b) {
		x = a;
		y = b;
	}

	public boolean within(int a, int b) {
		if (a >= x - PT_SIZE && 
			b >= y - PT_SIZE &&
			a <= x + PT_SIZE && 
			b <= y + PT_SIZE)
			return true;
		else
			return false;
	}
}

class CurvePanel extends Panel {
	public static final int HERMITE = 0;
	public static final int BEZIER = 1;
	public static final int BSPLINE = 2;
	private int mode = HERMITE;

	public static final int ADD = 0;
	public static final int MOVE = 1;
	public static final int DELETE = 2;
	private int action = ADD;

	private Vector points = new Vector(16,4);

	// If a control point is being moved, this is the index into the list
	// of the moving point.  Otherwise it contains -1
	private int moving_point;
	private int precision;
	private float eMatrix[][] = new float[4][4];

	// Initialize the curve-type matrices
	private static float bezierMatrix[][] = {
		{-1, 3,-3, 1},
		{ 3,-6, 3, 0},
		{-3, 3, 0, 0},
		{ 1, 0, 0, 0},
	};
	
	private static float hermiteMatrix[][] = {
		{ 2,-2, 1, 1},
		{-3, 3,-2,-1},
		{ 0, 0, 1, 0},
		{ 1, 0, 0, 0}
	};

	private static float mult = (float)(1.0/6.0);
	private static float bsplineMatrix[][] = {
		{    -mult, 3 * mult,-3 * mult, mult},
		{ 3 * mult,-6 * mult, 3 * mult,    0},
		{-3 * mult,        0, 3 * mult,    0},
		{     mult, 4 * mult,     mult,    0}
	};

	public CurvePanel() {
		setBackground(Color.white);
	}

	private void calcEMatrix(int prec) {
	// In order to use the "forward difference" method of curve plotting,
	// we must generate this matrix.  The parameter indicates the precision;
	// the number of line segments to use for each curve.

	float step = (float) (1.0/(float)prec);

		eMatrix[0][0] = 0;
		eMatrix[0][1] = 0;
		eMatrix[0][2] = 0;
		eMatrix[0][3] = 1;
	
		eMatrix[1][2] = step;
		eMatrix[1][1] = eMatrix[1][2] * step;
		eMatrix[1][0] = eMatrix[1][1] * step;
		eMatrix[1][3] = 0;

		eMatrix[2][0] = 6 * eMatrix[1][0]; 
		eMatrix[2][1] = 2 * eMatrix[1][1];
		eMatrix[2][2] = 0;
		eMatrix[2][3] = 0;

		eMatrix[3][0] = eMatrix[2][0];
		eMatrix[3][1] = 0;
		eMatrix[3][2] = 0;
		eMatrix[3][3] = 0;
	}

	public void setAction(int action) {
	// Change the action type
		switch (action) {
			case ADD:
			case MOVE:
			case DELETE:
				this.action = action;
				break;
			default:
				throw new IllegalArgumentException();
		}
	}

	public void setCurveType(int mode) {
	// Change the curve display type
		switch (mode) {
			case HERMITE:
			case BEZIER:
			case BSPLINE:
				this.mode = mode;
				break;
			default:
				throw new IllegalArgumentException();
		}
	}

	public void setPrecision(int prec) {
		precision = prec;
		calcEMatrix(prec);
	}

	public void clearPoints() {
		points.removeAllElements();
	}

	private int findPoint(int a, int b) {
	// Scan the list of control points to find out which (if any) point
	// contains the coordinates: a,b.
	// If a point is found, return the point's index, otherwise return -1
	int max = points.size();

		for(int i = 0; i < max; i++) {
			ControlPoint pnt = (ControlPoint)points.elementAt(i);
			if (pnt.within(a,b)) {
				return i;
			}
		}
		return -1;
	}

	public boolean handleEvent(Event e) {
		switch (e.id) {
			case Event.MOUSE_DOWN:
				// How we handle a MOUSE_DOWN depends on the action mode
				switch (action) {
					case ADD:
					// Add a new control point at the specified location
						ControlPoint pnt;
						points.addElement(pnt = new ControlPoint(e.x, e.y));
						repaint();
						break;
					case MOVE:
					// Attempt to select the point at the location specified.
					// If there is no point at the location, findPoint returns
					// -1 (i.e. there is no point to be moved)
						moving_point = findPoint(e.x, e.y);
						break;
					case DELETE:
					// Delete a point if one has been clicked
						int delete_pt = findPoint(e.x, e.y);
						if(delete_pt >= 0) {
							points.removeElementAt(delete_pt);
							repaint();
						}
						break;
					default:
						throw new IllegalArgumentException();
				}
				return true;
			case Event.MOUSE_UP:
			// We only care about MOUSE_UP's if we've been moving a control
			// point.  If so, drop the control point.
				if (moving_point >=0) {
					moving_point = -1;
					repaint();
				}
				return true;
			case Event.MOUSE_DRAG:
			// We only care about MOUSE_DRAG's while we are moving a control
			// point.  Otherwise, do nothing.
				if (moving_point >=0) {
					ControlPoint pnt = (ControlPoint) points.elementAt(moving_point);
					pnt.x = e.x;
					pnt.y = e.y;
					repaint();
				}
				return true;
			default:
				return false;
		}
	}

	private void multMatrix(float m[][], float g[][], float mg[][]) {
	// This function performs the meat of the calculations for the
	// curve plotting.  Note that it is not a matrix multiplier in the
	// pure sense.  The first matrix is the curve matrix (each curve type
	// has its own matrix), and the second matrix is the geometry matrix
	// (defined by the control points).  The result is returned in the
	// third matrix.

	// First clear the return array
		for(int i=0; i<4; i++) 
			for(int j=0; j<2; j++) 
				mg[i][j]=0;

	// Perform the matrix math
		for(int i=0; i<4; i++) 
			for(int j=0; j<2; j++) 
				for(int k=0; k<4; k++) 
					mg[i][j]=mg[i][j] + (m[i][k] * g[k][j]);
	}

	public void paint(Graphics g) {
	int np = points.size();
	float geom[][] = new float[4][2];
	float mg[][] = new float[4][2];
	float plot[][] = new float[4][2];

		g.setColor(getForeground());
		g.setPaintMode();

		// draw a border around the canvas
		g.drawRect(0,0, size().width-1, size().height-1);

		// draw the control points
		for (int i=0; i < np; i++) {
			ControlPoint p = (ControlPoint)points.elementAt(i);
			g.drawRect(p.x-p.PT_SIZE, p.y-p.PT_SIZE, p.PT_SIZE*2, p.PT_SIZE*2);
			g.drawString(String.valueOf(i),p.x+p.PT_SIZE,p.y-p.PT_SIZE);
		}

		for(int i = 0; i < np-3;) {
		// Four control points are needed to create a curve.
		// If all the control points are used, the last series of four 
		// points begins with point np-4.
			switch (mode) {
			// The geometry matrix for a series of control points is
			// different for each curve type.
				case(HERMITE):
					geom[0][0] = ((ControlPoint)points.elementAt(i)).x;
					geom[0][1] = ((ControlPoint)points.elementAt(i)).y;
					geom[1][0] = ((ControlPoint)points.elementAt(i+3)).x;
					geom[1][1] = ((ControlPoint)points.elementAt(i+3)).y;
					geom[2][0] = ((ControlPoint)points.elementAt(i+1)).x-geom[0][0];
					geom[2][1] = ((ControlPoint)points.elementAt(i+1)).y-geom[0][1];
					geom[3][0] = geom[1][0]-((ControlPoint)points.elementAt(i+2)).x;
					geom[3][1] = geom[1][1]-((ControlPoint)points.elementAt(i+2)).y;
					multMatrix(hermiteMatrix, geom, mg);
					// The beginning of the next Hermite curve is the last
					// point of the previous curve.
					i += 3;
					break;
				case(BEZIER):
					for(int j = 0; j <4 ;j++) {
						geom[j][0] = ((ControlPoint)points.elementAt(i+j)).x;
						geom[j][1] = ((ControlPoint)points.elementAt(i+j)).y;
					}
					multMatrix(bezierMatrix, geom, mg);
					// The beginning of the next Bezier curve is the last
					// point of the previous curve.
					i += 3;
					break;
				case(BSPLINE):
					for(int j = 3; j >= 0; j--) {
						geom[3-j][0] = ((ControlPoint)points.elementAt(i+j)).x;
						geom[3-j][1] = ((ControlPoint)points.elementAt(i+j)).y;
					}	
					multMatrix(bsplineMatrix, geom, mg);
					// B-Spline is the slowest curve, since the beginning of
					// the next series of four control points is the second
					// control point of the previous series.
					i++;
					break;
			}

			// In order to plot the curve using forward differences
			// (a speedier way to plot the curve), another matrix
			// calculation is required, taking into account the precision
			// of the curve.
			multMatrix(eMatrix, mg, plot);
			float startX = plot[0][0];
			float x = startX;
			float startY = plot[0][1];
			float y = startY;
			// Plot the curve using the forward difference method
			for(int j=0; j<precision; j++) {
				x += plot[1][0];
				plot[1][0] += plot[2][0];
				plot[2][0] += plot[3][0];
				y += plot[1][1];
				plot[1][1] += plot[2][1];
				plot[2][1] += plot[3][1];
				g.drawLine((int)startX,(int)startY,(int)x,(int)y);
				startX = x;
				startY = y;
			}
		}
	}
}

class CurveControls extends Panel {
CurvePanel target;
Checkbox cb_add;
Checkbox cb_move;
Checkbox cb_delete;
String st_add_label = "Add Points";
String st_move_label = "Move Points";
String st_delete_label = "Delete Points";

	public CurveControls(CurvePanel target) {
		this.target = target;
		setLayout(new FlowLayout(FlowLayout.CENTER));
		setBackground(Color.lightGray);
		Button clear = new Button("Clear");
		add("West",clear);

		CheckboxGroup action_group = new CheckboxGroup();
		add(cb_add = new Checkbox(st_add_label, action_group, true));
		add(cb_move = new Checkbox(st_move_label, action_group, false));
		add(cb_delete = new Checkbox(st_delete_label, action_group, false));
	}

	public void paint(Graphics g) {
	Rectangle r = bounds();

		g.setColor(Color.lightGray);
		g.draw3DRect(0, 0, r.width, r.height, false);
	}

	public boolean action(Event e, Object arg) {
		if (e.target instanceof Checkbox) {
			String cbox = ((Checkbox)(e.target)).getLabel();
			if (cbox.equals(st_add_label)) {
			target.setAction(CurvePanel.ADD);
		} else if (cbox.equals(st_move_label)) {
			target.setAction(CurvePanel.MOVE);
		} else if (cbox.equals(st_delete_label)) {
			target.setAction(CurvePanel.DELETE);
		}
	} else if (e.target instanceof Button) {
		String button = ((Button)(e.target)).getLabel();
		if (button.equals("Clear")) {
			target.clearPoints();

		// After clearing the control points, put the user back into
		// ADD mode, since none of the other modes make any sense.
		cb_add.setState(true);
		cb_delete.setState(false);
		cb_move.setState(false);
			target.setAction(CurvePanel.ADD);
		target.repaint();
		}
	}
	return true;
	}
}

class CurveControls2 extends Panel {
CurvePanel target;
Checkbox cb_hermite;
Checkbox cb_bezier;
Checkbox cb_bspline;
String st_hermite_label = "Hermite";
String st_bezier_label = "Bezier";
String st_bspline_label = "B-Spline";
Scrollbar scroll;
Label precision_display;

	public CurveControls2(CurvePanel target) {
		this.target = target;
		GridBagLayout gridbag = new GridBagLayout();
		setLayout(gridbag);
		GridBagConstraints c = new GridBagConstraints();

		c.fill = GridBagConstraints.HORIZONTAL;
		c.weightx = 0.0;
		c.weighty = 0.0;
		c.gridwidth = 1;
		c.gridheight = 1;
		CheckboxGroup type_group = new CheckboxGroup();

		cb_hermite = new Checkbox(st_hermite_label, type_group, true);
		gridbag.setConstraints(cb_hermite,c);
		add(cb_hermite);

		cb_bezier = new Checkbox(st_bezier_label, type_group, false);
		gridbag.setConstraints(cb_bezier,c);
		add(cb_bezier);

		cb_bspline = new Checkbox(st_bspline_label, type_group, false);
		gridbag.setConstraints(cb_bspline,c);
		add(cb_bspline);

		// Set up the scrollbar: Value 15, Page 5, Min 2, Max 40
		scroll = new Scrollbar(Scrollbar.HORIZONTAL, 15, 5, 2, 40);
		c.gridwidth = GridBagConstraints.RELATIVE;
		c.weightx = 1.0;
		gridbag.setConstraints(scroll,c);
		add(scroll);
		target.setPrecision(scroll.getValue());

		precision_display = new Label("Precision: "+String.valueOf(scroll.getValue()),Label.LEFT);
		c.gridwidth = 1;
		c.weightx = 0.0;
		gridbag.setConstraints(precision_display,c);
		add(precision_display);
	}

	public void paint(Graphics g) {
	Rectangle r = bounds();

		g.setColor(Color.lightGray);
		g.draw3DRect(0, 0, r.width, r.height, false);
	}

	public boolean handleEvent(Event e) {
		switch (e.id) {
			case Event.SCROLL_LINE_DOWN:
			case Event.SCROLL_LINE_UP:
			case Event.SCROLL_PAGE_DOWN:
			case Event.SCROLL_PAGE_UP:
			case Event.SCROLL_ABSOLUTE:
			// For any of these events, get the precision value from the
			// scrollbar, and update the curve precision, and the label.
				target.setPrecision(((Scrollbar)e.target).getValue());
				precision_display.setText("Precision: "+
					String.valueOf(((Scrollbar)e.target).getValue()));
				target.repaint();
				return true;
			case Event.ACTION_EVENT:
			// Handle other action events
				if (e.target instanceof Checkbox) {
					String cbox = ((Checkbox)(e.target)).getLabel();
					if (cbox.equals(st_hermite_label)) {
						target.setCurveType(CurvePanel.HERMITE);
					} else if (cbox.equals(st_bezier_label)) {
						target.setCurveType(CurvePanel.BEZIER);
					} else if (cbox.equals(st_bspline_label)) {
						target.setCurveType(CurvePanel.BSPLINE);
					}
				}
				target.repaint();
				return true;
			default:
				return false;
		}
	}
}
