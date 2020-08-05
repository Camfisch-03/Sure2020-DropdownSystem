package sure;
import java.util.*;

public class test03 {

	public static void main(String[] args) {
		Scanner sc = new Scanner(System.in);
		double r_earth = 6371000.0;
		
		//input data
		//System.out.println("Enter negative values for south or west headings");
		//System.out.print("Enter the degree value for latitude 1: ");
		//double lat1 = sc.nextDouble();
		//System.out.print("Enter the degree value for longitude 1: ");
		//double lon1 = sc.nextDouble();
		//System.out.print("Enter altitude 1 in meters: ");
		//double alt1 = sc.nextDouble();
		//System.out.print("Enter the degree value for latitude 2: ");
		//double lat2 = sc.nextDouble();
		//System.out.print("Enter the degree value for longitude 2: ");
		//double lon2 = sc.nextDouble();
		//System.out.print("Enter altitude 2 in meters: ");
		//double alt2 = sc.nextDouble();
		double lat1 = 42.6238; // latitude of position 1
		double lon1 = -87.8198; //longirude of position 1
		double alt1 = 90; //altitude above sea level of position 1
		double lat2 = -27.709588; //latitude of position 2
		double lon2 = 114.166432;	//altitude of position 2
		double alt2 = 10;	//altitude above sea level of position 2
		System.out.println("From position 1, of " + lat1 + ", " + lon1 + " at " + alt1 + " maters above sea level");
		System.out.println("to position 2, of: " + lat2 + ", " + lon2 + " at " + alt2 + " maters above sea level");
		
		//convert lat and lon to radians
		lat1 = lat1 * (Math.PI/180.0);
		lon1 = lon1 * (Math.PI/180.0);
		lat2 = lat2 * (Math.PI/180.0);
		lon2 = lon2 * (Math.PI/180.0);
		
		//calculate bearing
		double theta = (180/Math.PI) * Math.atan2(Math.cos(lat2) * Math.sin(lon2-lon1),Math.cos(lat1) * Math.sin(lat2) - Math.sin(lat1) * Math.cos(lat2) * Math.cos(lon2-lon1));
		if(theta < 0) {
			System.out.println((360+theta) + " degrees clockwise from north");
		}else {
			System.out.println(theta + " degrees clockwise from north");
		}
		
		//calculate distance (in meters)
		double gamma = Math.acos(Math.sin(lat1) * Math.sin(lat2) + Math.cos(lat1) * Math.cos(lat2) * Math.cos(lon2 - lon1));
			//gamma is actually the angle between the 2 points
		//System.out.println(r_earth * gamma + " meters away by land"); //r*gamma is the arc length between the points
		
		//calculate angle above horizon
		
		//assign values to the side lengths of a triangle
		double a = r_earth + alt1;// from center of earth to location 1
		double b = r_earth + alt2; //from center of earth to location 2
		double c = Math.sqrt(a*a + b*b - 2 * a * b * Math.cos(gamma));
		
		double beta = Math.acos((a*a+c*c-b*b)/(2*a*c)); //solve for the angle of beta
		double deg = -90 + beta*(180/Math.PI);	//beta relative to the horizontal
		System.out.println(deg + " Degrees above the horizon");
		
		System.out.println(r_earth * gamma + " meters away by land");
		
	}

}
