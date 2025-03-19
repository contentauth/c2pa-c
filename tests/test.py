import c2pa
print("c2pa version:")
version = c2pa.version()
print(version)
# Read a manifest from a file
manifest = c2pa.read_file("tests/fixtures/C.jpg")
print(manifest)

reader = c2pa.Reader("tests/fixtures/C.jpg")
print(reader.json())

#ingredient = c2pa.read_ingredient_file("tests/fixtures/C.jpg")
#print(ingredient)

signer_info = c2pa.C2paSignerInfo(
    alg="es256",
    sign_cert="cert.pem",
    private_key="key.pem",
    ta_url=None
)
signer = c2pa.signer_from_info(signer_info)

builder = c2pa.Builder.from_json('{ }')
#builder.add_ingredient("tests/fixtures/C.jpg", "image/jpeg")
#builder.add_resource("thumbnail.jpg", "tests/fixtures/A.jpg")
#result =builder.sign("tests/fixtures/C.jpg", "tests/fixtures/C_signed.jpg")


# Create a signer
#signer_info = c2pa.C2paSignerInfo(
#    alg="es256",
#    sign_cert="cert.pem",
#    private_key="key.pem",
#    ta_url=None
#)
#signer = c2pa.signer_from_info(signer_info)

# Sign a file
#result = c2pa.sign_file(
#``    source_path="input.jpg",
#    dest_path="output.jpg",
 #   manifest='{"claim": {...}}',
 #   signer_info=signer_info
#)

# Create a builder for more complex operations
#with c2pa.Builder.from_json('{"claim": {...}}') as builder:
#   # Add resources and ingredients
#    builder.add_resource("thumbnail.jpg", thumbnail_stream)
#    builder.add_ingredient(ingredient_json, "image/jpeg", source_stream)
    
    # Sign the content
#    size, manifest_bytes = builder.sign(
#        format="image/jpeg",
#        source=source_stream,
#        dest=dest_stream,
#        signer=signer
#    )