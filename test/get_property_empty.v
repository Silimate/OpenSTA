// Every port is an input, so all_outputs comes back empty.
module no_outputs (in1, in2, clk);
  input in1, in2, clk;
  wire r1q, u1z;

  DFF_X1 r1 (.D(in1), .CK(clk), .Q(r1q));
  BUF_X1 u1 (.A(r1q), .Z(u1z));
endmodule // no_outputs
