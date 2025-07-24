module top (in1, in2, in3, clk1, clk2, clk3, out1, out2);
  
  input in1, in2, in3, clk1, clk2, clk3;
  output out1, out2;
  wire w1, w2, w3;

  timing_test tt1(.in1(in1), .in2(in2), .in3(in3), .clk1(clk1), .clk2(clk2), .clk3(clk3), .out1(w1), .out2(w2));

  BUF_X1 b1 (.A(w1), .Z(w3));

  timing_test tt2(.in1(w3), .in2(w2), .in3(in3), .clk1(clk1), .clk2(clk2), .clk3(clk3), .out1(out1), .out2(out2));

endmodule // top
