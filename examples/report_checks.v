module top (in, clk, out);
  
  input in, clk;
  output out;
  wire w1, w2, w3;

  timing_paths tt1(.in(in), .clk(clk), .out(w1));

  BUF_X1 b1 (.A(w1), .Z(w2));

  timing_paths tt2(.in(w2), .clk(clk), .out(out));

endmodule // top
