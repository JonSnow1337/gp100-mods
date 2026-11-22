// List instructions that compare a register with a given immediate (e.g. #0x1d, #0x1b) and the functions containing them.
// Usage: -postScript FindCmp.java OUT_FILE IMM [IMM...]  (read-only)
// @category GP100
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import java.io.PrintWriter;

public class FindCmp extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        try (PrintWriter out = new PrintWriter(a[0])) {
            InstructionIterator it = currentProgram.getListing().getInstructions(true);
            while (it.hasNext()) {
                Instruction ins = it.next();
                String m = ins.getMnemonicString().toLowerCase();
                if (!(m.startsWith("cmp") || m.startsWith("sub") || m.startsWith("mov") || m.startsWith("teq"))) continue;
                String s = ins.toString().toLowerCase();
                for (int i = 1; i < a.length; i++) {
                    if (s.endsWith("#" + a[i].toLowerCase()) && m.startsWith("cmp")) {
                        Function f = getFunctionContaining(ins.getAddress());
                        byte[] b = ins.getBytes();
                        StringBuilder hex = new StringBuilder();
                        for (byte x : b) hex.append(String.format("%02x", x));
                        out.printf("%s  %-22s %s  in %s%n", ins.getAddress(), s, hex, f == null ? "-" : f.getName());
                    }
                }
            }
        }
    }
}
