import("stdfaust.lib");

level = (vslider("level", 0.5, 0, 10, 0.01):si.smooth(0.993))*0.2;
freq = (vslider("frequency",5,0.5,20,0.01) : si.smooth(0.993))/60;
depth = (min((freq/4),1))*(pow(10,(vslider("depth",0.25,0,0.5,0.01):si.smooth(0.993))*0.1)/10-0.1);
w = 512;
x = 256;

process =  ef.transpose(w, x, depth*os.osc(freq)):*(level);
