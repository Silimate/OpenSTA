(* src = "counter.v:16.1-32.10|pqid:m0" *)
module counter(clk, in, out);
  input clk;
  output out;
  input in;
  wire mid;
  wire mid2;
  (* src = "counter.v:22.3-28.6|pqid:a1|counter.v:22.3-28.6|pqid:b2", attr1 = "pqid:n1" *)
  DFFHQx4_ASAP7_75t_R _1415_ (
    .CLK(clk),
    .D(in),
    .Q(mid)
  );
  (* src = "pqid:c3" *)
  DFFHQx4_ASAP7_75t_R _1416_ (
    .CLK(clk),
    .D(mid),
    .Q(mid2)
  );
  (* src = "counter.v:30.1-30.5|counter.v:30.1-30.5" *)
  DFFHQx4_ASAP7_75t_R _1417_ (
    .CLK(clk),
    .D(mid2),
    .Q(out)
  );
endmodule
