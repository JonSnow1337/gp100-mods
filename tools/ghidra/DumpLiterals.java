// Collect 32-bit constants loaded by decoded instructions (literal-pool loads), bucket them by
// address region, and list where selected regions are used. Read-only.
// Usage: -postScript DumpLiterals.java OUT_FILE
// @category GP100
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import java.io.PrintWriter;
import java.util.Map;
import java.util.TreeMap;

public class DumpLiterals extends GhidraScript {
    @Override
    public void run() throws Exception {
        String outPath = getScriptArgs()[0];
        Map<Long, Integer> byMeg = new TreeMap<>();      // value >> 20
        Map<Long, Integer> periph = new TreeMap<>();     // 0x40000000-0x5fffffff, value & ~0xfff
        Map<Long, StringBuilder> high = new TreeMap<>(); // 0x60000000-0x9fffffff exact value -> users
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext() && !monitor.isCancelled()) {
            Instruction ins = it.next();
            if (!ins.getMnemonicString().toLowerCase().startsWith("ldr")) continue;
            for (Reference r : ins.getReferencesFrom()) {
                Address to = r.getToAddress();
                if (!to.isMemoryAddress() || !currentProgram.getMemory().contains(to)) continue;
                long v;
                try { v = currentProgram.getMemory().getInt(to) & 0xffffffffL; } catch (Exception e) { continue; }
                byMeg.merge(v >> 20, 1, Integer::sum);
                if (v >= 0x40000000L && v < 0x60000000L) periph.merge(v & ~0xfffL, 1, Integer::sum);
                if (v >= 0x60000000L && v < 0xa0000000L) {
                    Function f = getFunctionContaining(ins.getAddress());
                    StringBuilder sb = high.computeIfAbsent(v, k -> new StringBuilder());
                    if (sb.length() < 200) sb.append(' ').append(ins.getAddress()).append(f == null ? "" : "(" + f.getName() + ")");
                }
            }
        }
        try (PrintWriter out = new PrintWriter(outPath)) {
            out.println("== loaded constants by 1MB region (value>>20 : count)");
            for (Map.Entry<Long, Integer> e : byMeg.entrySet())
                if (e.getValue() >= 3) out.printf("  0x%08x %d%n", e.getKey() << 20, e.getValue());
            out.println("== peripheral pages 0x4xxxxxxx/0x5xxxxxxx");
            for (Map.Entry<Long, Integer> e : periph.entrySet()) out.printf("  0x%08x %d%n", e.getKey(), e.getValue());
            out.println("== 0x60000000-0x9fffffff constants and users");
            for (Map.Entry<Long, StringBuilder> e : high.entrySet()) out.printf("  0x%08x%s%n", e.getKey(), e.getValue());
        }
    }
}
