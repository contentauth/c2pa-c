fn main() {
    uniffi::generate_scaffolding("src/c2pa_c.udl").expect("Failed to generate scaffolding");
}
