package sure;
import java.util.*;

public class ang_dist_calc_v1 {

	public static void main(String[] args) {
		// define constant values
		double r_earth = 6371000.0;	//radius in meters of earth

		// set position data
		double lat1 = 42.036256; // latitude of position 1
		double lon1 = -88.195639; // longitude of position 1
		double alt1 = 260; // altitude above sea level of position 1
		double lat2 = 42.036415; // latitude of position 2
		double lon2 = -88.195671; // altitude of position 2
		double alt2 = 500; // altitude above sea level of position 2
		System.out.println("From position 1, of " + lat1 + ", " + lon1 + " at " + alt1 + " maters above sea level");
		System.out.println("to position 2, of: " + lat2 + ", " + lon2 + " at " + alt2 + " maters above sea level\n");

		// convert lat and lon to radians from degrees
		lat1 = lat1 * (Math.PI / 180.0);
		lon1 = lon1 * (Math.PI / 180.0);
		lat2 = lat2 * (Math.PI / 180.0);
		lon2 = lon2 * (Math.PI / 180.0);

		// calculate bearing angle
		double theta = (180 / Math.PI) * Math.atan2(Math.cos(lat2) * Math.sin(lon2 - lon1),
				Math.cos(lat1) * Math.sin(lat2) - Math.sin(lat1) * Math.cos(lat2) * Math.cos(lon2 - lon1));
		if (theta < 0) {
			System.out.println((360 + theta) + " degrees clockwise from north");
		} else {
			System.out.println(theta + " degrees clockwise from north");
		}

		// calculate angle of seperation
		double gamma = Math
				.acos(Math.sin(lat1) * Math.sin(lat2) + Math.cos(lat1) * Math.cos(lat2) * Math.cos(lon2 - lon1));

		// assign values to the side lengths of a triangle
		double a = r_earth + alt1;// from center of earth to location 1
		double b = r_earth + alt2; // from center of earth to location 2
		double c = Math.sqrt(a * a + b * b - 2 * a * b * Math.cos(gamma));
		
		//calculate angle above horizon
		double beta = Math.acos((a*a+c*c-b*b)/(2*a*c)); //solve for the angle of beta
		double deg = -90 + beta*(180/Math.PI);	//beta relative to the horizontal
		System.out.println(deg + " Degrees above the horizon");
		
		//calculate absolute distance from point to point
		//i didnt even realize, but i already did that, its just c
		System.out.println(c + " meters away by a straight line");
		
		//calculate distance away by land
		System.out.println( r_earth * gamma + " meters away by land"); //raduis times angle is arclength
		

	}

}
