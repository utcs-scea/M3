use cap::{Flags, Selector};
use com::gate::Gate;
use com::RecvGate;
use dtu;
use errors::Error;
use kif::INVALID_SEL;
use syscalls;
use util;
use vpe;

#[derive(Debug)]
pub struct SendGate {
    gate: Gate,
}

pub struct SGateArgs {
    rgate_sel: Selector,
    label: dtu::Label,
    credits: u64,
    sel: Selector,
    flags: Flags,
}

impl SGateArgs {
    pub fn new(rgate: &RecvGate) -> Self {
        SGateArgs {
            rgate_sel: rgate.sel(),
            label: 0,
            credits: 0,
            sel: INVALID_SEL,
            flags: Flags::empty(),
        }
    }

    pub fn credits(mut self, credits: u64) -> Self {
        self.credits = credits;
        self
    }

    pub fn label(mut self, label: dtu::Label) -> Self {
        self.label = label;
        self
    }

    pub fn sel(mut self, sel: Selector) -> Self {
        self.sel = sel;
        self
    }
}

impl SendGate {
    pub fn new(rgate: &RecvGate) -> Result<Self, Error> {
        Self::new_with(SGateArgs::new(rgate))
    }

    pub fn new_with(args: SGateArgs) -> Result<Self, Error> {
        let sel = if args.sel == INVALID_SEL {
            vpe::VPE::cur().alloc_cap()
        }
        else {
            args.sel
        };

        try!(syscalls::create_sgate(sel, args.rgate_sel, args.label, args.credits));
        Ok(SendGate {
            gate: Gate::new(sel, args.flags),
        })
    }

    pub fn new_bind(sel: Selector) -> Self {
        SendGate {
            gate: Gate::new(sel, Flags::KEEP_CAP),
        }
    }

    pub fn sel(&self) -> Selector {
        self.gate.sel()
    }

    pub fn ep(&self) -> Option<dtu::EpId> {
        self.gate.ep()
    }

    pub fn rebind(&mut self, sel: Selector) -> Result<(), Error> {
        self.gate.rebind(sel)
    }

    pub fn send<T>(&self, msg: &[T], reply_gate: &RecvGate) -> Result<(), Error> {
        self.send_bytes(msg.as_ptr() as *const u8, msg.len() * util::size_of::<T>(), reply_gate)
    }
    pub fn send_bytes(&self, msg: *const u8, size: usize, reply_gate: &RecvGate) -> Result<(), Error> {
        let ep = try!(self.gate.activate());
        dtu::DTU::send(ep, msg, size, 0, reply_gate.ep().unwrap())
    }
}

pub mod tests {
    use super::*;
    use collections::String;
    use com;
    use util;

    pub fn run(t: &mut ::test::Tester) {
        run_test!(t, create);
        run_test!(t, send_recv);
        run_test!(t, send_reply);
    }

    fn create() {
        let rgate = assert_ok!(RecvGate::new(util::next_log2(512), util::next_log2(256)));
        assert_err!(SendGate::new_with(SGateArgs::new(&rgate).sel(1)), Error::InvArgs);
    }

    fn send_recv() {
        let mut rgate = assert_ok!(RecvGate::new(util::next_log2(512), util::next_log2(256)));
        let sgate = assert_ok!(SendGate::new_with(
            SGateArgs::new(&rgate).credits(512).label(0x1234)
        ));
        assert!(sgate.ep().is_none());
        assert_ok!(rgate.activate());

        let data = [0u8; 16];
        assert_ok!(sgate.send(&data, RecvGate::def()));
        assert!(sgate.ep().is_some());
        assert_ok!(sgate.send(&data, RecvGate::def()));
        assert_err!(sgate.send(&data, RecvGate::def()), Error::MissCredits);

        {
            let msg = assert_ok!(rgate.wait(Some(&sgate)));
            assert_eq!(msg.header.label, 0x1234);
            dtu::DTU::mark_read(rgate.ep().unwrap(), msg);
        }

        {
            let msg = assert_ok!(rgate.wait(Some(&sgate)));
            assert_eq!(msg.header.label, 0x1234);
            dtu::DTU::mark_read(rgate.ep().unwrap(), msg);
        }
    }

    fn send_reply() {
        let reply_gate = RecvGate::def();
        let mut rgate = assert_ok!(RecvGate::new(util::next_log2(64), util::next_log2(64)));
        let sgate = assert_ok!(SendGate::new_with(
            SGateArgs::new(&rgate).credits(64).label(0x1234)
        ));
        assert!(sgate.ep().is_none());
        assert_ok!(rgate.activate());

        assert_ok!(send_vmsg!(&sgate, &reply_gate, 0x123, 12, "test"));

        // sgate -> rgate
        {
            let mut msg = assert_ok!(com::recv_msg(&rgate));
            let (i1, i2, s): (i32, i32, String) = (msg.pop(), msg.pop(), msg.pop());
            assert_eq!(i1, 0x123);
            assert_eq!(i2, 12);
            assert_eq!(s, "test");

            assert_ok!(reply_vmsg!(msg, 44, 3));
        }

        // rgate -> reply_gate
        {
            let mut reply = assert_ok!(com::recv_msg_from(&reply_gate, Some(&sgate)));
            let (i1, i2): (i32, i32) = (reply.pop(), reply.pop());
            assert_eq!(i1, 44);
            assert_eq!(i2, 3);
        }
    }
}
