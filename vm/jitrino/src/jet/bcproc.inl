/**
 * @author Alexander V. Astapchuk
 * @version $Revision: 1.4.12.2.4.1 $
 */

/**
 * @file
 * @brief In-lined version of Compiler::fetch().
 */
namespace Jitrino { 
namespace Jet {

inline unsigned Compiler::fetch(unsigned pc, JInst& jinst)
{
    if(pc >= m_infoBlock.get_bc_size()) {
        return NOTHING;
    }
    
    jinst.op0 = NOTHING;
    jinst.op1 = NOTHING;
    
    jinst.pc = pc;
    jinst.opcode = (JavaByteCodes)m_bc[pc];
    ++pc;
    const InstrDesc& idesc = instrs[jinst.opcode];
    jinst.flags = idesc.flags;
    
    switch (idesc.len) {
        case 0:
            if (jinst.opcode == OPCODE_WIDE) {
                jinst.opcode = (JavaByteCodes)m_bc[pc];
                const unsigned b1 = m_bc[pc+1];
                const unsigned b2 = m_bc[pc+2];
                jinst.op0 = b1<<8 | b2;
                pc += 3;
                if (jinst.opcode == OPCODE_IINC) {
                    const unsigned b1 = m_bc[pc+0];
                    const unsigned b2 = m_bc[pc+1];
                    // sign extend it
                    jinst.op1 = (int)(short)(b1<<8 | b2);
                    pc += 2;
                }
            }
            else {
                assert(jinst.opcode == OPCODE_TABLESWITCH || 
                       jinst.opcode == OPCODE_LOOKUPSWITCH);
                // data is aligned on 4 bytes boundary
                unsigned data_pc = (unsigned)((pc+3)&~3);
                jinst.data = m_bc + data_pc;
                pc = data_pc + jinst.get_data_len();
            }
            break;
        case 1: break;
        case 2: 
            jinst.op0 = m_bc[pc];
            ++pc;
            break;
        case 3:
            {
                const unsigned b1 = m_bc[pc+0];
                const unsigned b2 = m_bc[pc+1];
                if (jinst.opcode == OPCODE_IINC) {
                    jinst.op0 = b1;
                    // sign extend it.
                    jinst.op1 = (int)(char)m_bc[pc+1];;
                }
                else {
                    jinst.op0 = b1<<8 | b2;
                }
                pc += 2;
            }
            break;
        case 4:
            {
                // the only case is MULTINEWARRAY
                assert(jinst.opcode == OPCODE_MULTIANEWARRAY);
                const unsigned b1 = m_bc[pc+0];
                const unsigned b2 = m_bc[pc+1];
                jinst.op0 = b1<<8 | b2;
                jinst.op1 = m_bc[pc+2];
                pc += 3;
            }
            break;
        case 5:
            if (jinst.opcode == OPCODE_JSR_W || 
                jinst.opcode == OPCODE_GOTO_W) {
                const unsigned b1 = m_bc[pc+0];
                const unsigned b2 = m_bc[pc+1];
                const unsigned b3 = m_bc[pc+2];
                const unsigned b4 = m_bc[pc+3];
                jinst.op0 = (b1<<24)|(b2<<16)|(b3<<8)|b4;
                pc += 4;
            }
            else {
                // The only case is INVOKEINTEFACE
                assert(jinst.opcode == OPCODE_INVOKEINTERFACE);
                const unsigned b1 = m_bc[pc+0];
                const unsigned b2 = m_bc[pc+1];
                jinst.op0 = b1<<8 | b2;
                jinst.op1 = m_bc[pc+2];
                pc += 4;
            }
            break;
        default:
            assert(false); break;
    }
    jinst.next = pc;
    return pc;
}

}};	// ~namespace Jitrino::Jet


