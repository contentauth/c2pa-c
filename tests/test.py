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

data_dir = "tests/fixtures/"

certs = open(data_dir + "es256_certs.pem", "rb").read()
key = open(data_dir + "es256_private.key", "rb").read()

signer_info = c2pa.C2paSignerInfo(
    alg="es256".encode('utf-8'),
    sign_cert=certs,
    private_key=key,
    ta_url=None
)

signer = c2pa.create_signer_from_info(signer_info)

builder = c2pa.Builder.from_json('{ }')
#builder.add_ingredient("tests/fixtures/C.jpg", "image/jpeg")
#builder.add_resource("thumbnail.jpg", "tests/fixtures/A.jpg")
source = open("tests/fixtures/C.jpg", "rb");
dest = open("target/C_signed.jpg", "wb");
source = c2pa.Stream(source)
dest = c2pa.Stream(dest)    
result =builder.sign("image/jpeg", source, dest, signer)

reader = c2pa.Reader("target/C_signed.jpg")
print(reader.json())

