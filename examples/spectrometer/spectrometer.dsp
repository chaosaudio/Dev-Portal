declare version 	"0.1.0";
import("stdfaust.lib");

/* Used for inputs of 0 - 10
eq1 = vslider("eq1",0.5, 0, 1, 0.025)*2.4-12;
eq2 = vslider("eq2",0.5, 0, 1, 0.025)*2.4-12;
eq3 = vslider("eq3",0.5, 0, 1, 0.025)*2.4-12;
eq4 = vslider("eq4",0.5, 0, 1, 0.025)*2.4-12;
eq5 = vslider("eq5",0.5, 0, 1, 0.025)*2.4-12;
eq6 = vslider("eq6",0.5, 0, 1, 0.025)*2.4-12;
*/

/* Used for inputs of -12 - 12*/
eq1 = vslider("eq1",0.5, 0, 1, 0.025);
eq2 = vslider("eq2",0.5, 0, 1, 0.025);
eq3 = vslider("eq3",0.5, 0, 1, 0.025);
eq4 = vslider("eq4",0.5, 0, 1, 0.025);
eq5 = vslider("eq5",0.5, 0, 1, 0.025);
eq6 = vslider("eq6",0.5, 0, 1, 0.025);

fc1 = 100;
fc2 = 200;
fc3 = 400;
fc4 = 800;
fc5 = 1600;
fc6 = 3200;

bw1 = 100;
bw2 = 200;
bw3 = 400;
bw4 = 800;
bw5 = 1600;
bw6 = 3200;

filt1 = fi.peak_eq(eq1,fc1,bw1);
filt2 = fi.peak_eq(eq2,fc2,bw2);
filt3 = fi.peak_eq(eq3,fc3,bw3);
filt4 = fi.peak_eq(eq4,fc4,bw4);
filt5 = fi.peak_eq(eq5,fc5,bw5);
filt6 = fi.peak_eq(eq6,fc6,bw6);

process = filt1 : filt2: filt3 : filt4 : filt5 : filt6; 