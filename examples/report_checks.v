module top (in, clk, out);
  
  input in, clk;
  output out;
  wire w1, w2;

  timing_paths tt1(.in(in), .clk(clk), .out(w1));

  timing_paths_comb tt2(.in(w1), .out(w2));

  timing_paths tt3(.in(w2), .clk(clk), .out(out));

endmodule // top
