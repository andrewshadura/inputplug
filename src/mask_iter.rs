use std::iter::Iterator;

pub struct IterableMask<T> {
    value: T,
    curr_mask: T
}

macro_rules! implement_iterable_mask {
    ($t: ty) => {
        impl Iterator for IterableMask<$t> {
            type Item = $t;

            fn next(&mut self) -> Option<$t> {
                loop {
                    let bit = self.curr_mask & self.value;
                    self.curr_mask >>= 1;
                    if bit != 0 {
                        return Some(bit)
                    }
                    if self.curr_mask == 0 {
                        break
                    }
                }
                None
            }
        }

        impl From<$t> for IterableMask<$t> {
            fn from(value: $t) -> Self {
                IterableMask {
                    value: value,
                    curr_mask: <$t>::from(1u8).rotate_right(1)
                }
            }
        }
    }
}

implement_iterable_mask!(u8);
implement_iterable_mask!(u16);
implement_iterable_mask!(u32);

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn basic() {
        assert_eq!(
            IterableMask::from(0x123ce_u32).collect::<Vec<u32>>(),
            [0x10000, 0x2000, 0x200, 0x100, 0x80, 0x40, 8, 4, 2]
        );
        assert_eq!(
            IterableMask::from(0x23c5_u16).collect::<Vec<u16>>(),
            [0x2000, 0x200, 0x100, 0x80, 0x40, 4, 1]
        );
        assert_eq!(
            IterableMask::from(0xce_u8).collect::<Vec<u8>>(),
            [0x80, 0x40, 8, 4, 2]
        );
        assert_eq!(IterableMask::from(0_u8).collect::<Vec<u8>>(), []);
    }
}
