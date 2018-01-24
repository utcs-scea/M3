use m3::col::BTreeMap;
use m3::profile;
use m3::test;

pub fn run(t: &mut test::Tester) {
    run_test!(t, insert);
    run_test!(t, find);
    run_test!(t, clear);
}

fn insert() {
    let mut prof = profile::Profiler::new().repeats(30);

    #[derive(Default)]
    struct BTreeTester(BTreeMap<u32, u32>);

    impl profile::Runner for BTreeTester {
        fn pre(&mut self) {
            self.0.clear();
        }
        fn run(&mut self) {
            for i in 0..100 {
                self.0.insert(i, i);
            }
        }
    }

    println!("Inserting 100 elements: {}", prof.runner_with_id(&mut BTreeTester::default(), 0x81));
}

fn find() {
    let mut prof = profile::Profiler::new().repeats(30);

    #[derive(Default)]
    struct BTreeTester(BTreeMap<u32, u32>);

    impl profile::Runner for BTreeTester {
        fn pre(&mut self) {
            for i in 0..100 {
                self.0.insert(i, i);
            }
        }
        fn run(&mut self) {
            for i in 0..100 {
                assert_eq!(self.0.get(&i), Some(&i));
            }
        }
    }

    println!("Searching for 100 elements: {}", prof.runner_with_id(&mut BTreeTester::default(), 0x82));
}

fn clear() {
    let mut prof = profile::Profiler::new().repeats(30);

    #[derive(Default)]
    struct BTreeTester(BTreeMap<u32, u32>);

    impl profile::Runner for BTreeTester {
        fn pre(&mut self) {
            for i in 0..100 {
                self.0.insert(i, i);
            }
        }
        fn run(&mut self) {
            self.0.clear();
        }
    }

    println!("Removing 100-element list: {}", prof.runner_with_id(&mut BTreeTester::default(), 0x83));
}
