namespace nextgen{

template<class Exponent, class GroupElement>
void NextGenPrimitives<Exponent, GroupElement>::commit(const GroupElement& g,
        const zcoin_common::GeneratorVector<Exponent, GroupElement>& h,
        const std::vector<Exponent>& exp,
        const Exponent& r,
        GroupElement& result_out)  {
    result_out += g * r;
    h.get_vector_multiple(exp, result_out);
}

template<class Exponent, class GroupElement>
GroupElement NextGenPrimitives<Exponent, GroupElement>::commit(
        const GroupElement& g,
        const Exponent& m,
        const GroupElement& h,
        const Exponent& r){
    return g * m + h * r;
}

template<class Exponent, class GroupElement>
GroupElement NextGenPrimitives<Exponent, GroupElement>::double_commit(
        const GroupElement& g,
        const Exponent& m,
        const GroupElement& hV,
        const Exponent& v,
        const GroupElement& hR,
        const Exponent& r){
    GroupElement result;
    result += g*m;
    result += hV*v;
    result += hR*r;
    return result;
}

template<class Exponent, class GroupElement>
void NextGenPrimitives<Exponent, GroupElement>::convert_to_sigma(
        uint64_t num,
        uint64_t n,
        uint64_t m,
        std::vector<Exponent>& out){
    int rem, nalNumber = 0;
    int j = 0;

    while (num != 0)
    {
        rem = num % n;
        num /= n;
        for(int i = 0; i < n; ++i){
            if(i == rem)
                out.push_back(Exponent(uint64_t(1)));
            else
                out.push_back(Exponent(uint64_t(0)));
        }
        j++;
    }

    for(int k = j; k < m; ++k){
        out.push_back(Exponent(uint64_t(1)));
        for(int i = 1; i < n; ++i){
            out.push_back(Exponent(uint64_t(0)));
        }
    }
}

template<class Exponent, class GroupElement>
std::vector<uint64_t> NextGenPrimitives<Exponent, GroupElement>::convert_to_nal(
        uint64_t num,
        uint64_t n,
        uint64_t m){
    std::vector<uint64_t> result;
    uint64_t rem, nalNumber = 0;
    uint64_t j = 0;
    while (num != 0)
    {
        rem = num % n;
        num /= n;
        result.push_back(rem);
        j++;
    }
    result.resize(m);
    return result;
}

template<class Exponent, class GroupElement>
void NextGenPrimitives<Exponent, GroupElement>::get_x(
        const GroupElement& A,
        const GroupElement& C,
        const GroupElement& D,
        Exponent& result_out) {
    secp256k1_sha256_t hash;
    secp256k1_sha256_initialize(&hash);
    unsigned char data[3 * A.memoryRequired()];
    unsigned char* current = A.serialize(data);
    current = C.serialize(current);
    D.serialize(current);
    secp256k1_sha256_write(&hash, &data[0], 3 * 34);
    unsigned char result_data[32];
    secp256k1_sha256_finalize(&hash, result_data);
    result_out = result_data;

}

template<class Exponent, class GroupElement>
void  NextGenPrimitives<Exponent, GroupElement>::get_x(const std::vector<SigmaPlusProof<Exponent, GroupElement>>& proofs, Exponent& result_out) {
    if (proofs.size() > 0){
        secp256k1_sha256_t hash;
        secp256k1_sha256_initialize(&hash);
        unsigned char data[3 * proofs.size() * 34];
        unsigned char* current = data;
        for (int i = 0; i < proofs.size(); ++i) {
            current = proofs[i].A_.serialize(current);
            current = proofs[i].C_.serialize(current);
            current = proofs[i].D_.serialize(current);
        }
        secp256k1_sha256_write(&hash, data, 3 * proofs.size() * 34);
        unsigned char result_data[32];
        secp256k1_sha256_finalize(&hash, result_data);
        result_out = result_data;
    }
    else
        result_out = uint64_t(1);
}



template<class Exponent, class GroupElement>
void NextGenPrimitives<Exponent, GroupElement>::new_factor(
        Exponent x,
        Exponent a,
        std::vector<Exponent>& coefficients) {
    std::vector<Exponent> temp;
    temp.resize(coefficients.size() + 1);
    for(int j = 0; j < coefficients.size(); j++)
        temp[j] += x * coefficients[j];
    for(int j = 0; j < coefficients.size(); j++)
        temp[j + 1] += a * coefficients[j];
    coefficients = temp;
}

template<class Exponent, class GroupElement>
void NextGenPrimitives<Exponent, GroupElement>::get_c(const GroupElement& u, Exponent& result){
    secp256k1_sha256_t hash;
    secp256k1_sha256_initialize(&hash);
    unsigned char data[34];
    u.serialize(data);
    secp256k1_sha256_write(&hash, &data[0], 34);
    unsigned char result_data[32];
    secp256k1_sha256_finalize(&hash, result_data);
    result = result_data;
}

}//namespace nextgen