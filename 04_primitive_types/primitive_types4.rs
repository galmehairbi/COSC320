fn main() {
    let a = [10, 20, 30, 40, 50];

    let full_slice = &a[..];
    println!("full slice: {:?}", full_slice);

    let slice_one = &a[2..];
    println!("Slice from index 2: {:?}", slice_one);

    let slice_two = &a[..2];
    println!("Slice to index 2: {:?}", slice_two);

}

#[cfg(test)]
mod tests {
    #[test]
    fn slice_out_of_array() {
        let a = [1, 2, 3, 4, 5];

        // TODO: Get a slice called `nice_slice` out of the array `a` so that the test passes.
        // let nice_slice = ???

        let nice_slice = &a[1..4];
        
        println!("{:?}",nice_slice);

        assert_eq!([2, 3, 4], nice_slice);
    }
}