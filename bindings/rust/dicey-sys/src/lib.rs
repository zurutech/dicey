#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

#[cfg(test)]
mod tests {
    use std::ffi::CString;
    use std::mem;
    
    use super::*;

    #[test]
    fn test_dump_undump() {
        unsafe {
            let mut builder : dicey_message_builder = mem::zeroed();

            let err = dicey_message_builder_init(&mut builder);
            assert_eq!(err, 0);

            let err = dicey_message_builder_begin(&mut builder, dicey_op_DICEY_OP_SET);
            assert_eq!(err, 0);

            let err = dicey_message_builder_set_seq(&mut builder, 0);
            assert_eq!(err, 0);

            let path = CString::new("/sval").unwrap();
            let err = dicey_message_builder_set_path(&mut builder, path.as_ptr());
            assert_eq!(err, 0);

            let trait_ = CString::new("sval.Sval").unwrap();
            let elem   = CString::new("Value").unwrap();

            let err = dicey_message_builder_set_selector(&mut builder, dicey_selector {
                trait_: trait_.as_ptr(),
                elem:   elem.as_ptr(),
            });
            assert_eq!(err, 0);

            let msg = CString::new("hello there").unwrap();
            let arg = dicey_arg {
                type_: dicey_type_DICEY_TYPE_STR,
                __bindgen_anon_1: dicey_arg__bindgen_ty_1 {
                    str_: msg.as_ptr(),
                },
            };

            let err = dicey_message_builder_set_value(&mut builder, arg);
            assert_eq!(err, 0);

            let mut pkg = mem::zeroed();

            let err = dicey_message_builder_build(&mut builder, &mut pkg);
            assert_eq!(err, 0);

            dicey_packet_deinit(&mut pkg);
        }
    }
}