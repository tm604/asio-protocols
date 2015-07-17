# HTTP

Some basic implementation details:

## GET /path => empty

    return client_.get(uri)
        ->expect_status(200, 201, 202, 204)
        ->ignore_body()
        ->completion();

## GET /path => binary

    return client_.get(uri)
        ->expect_status(200, 201, 202, 204)
        ->stream_body([ca](const std::string &in) {
            return ca->append_body(in);
        })
        ->completion();

## GET /path => JSON

    return client_.get(uri)
        ->expect_status(200, 201, 202, 204)
        ->expect_content_type("application/javascript")
        ->completion();
    json::parse(resp->body());

## JSON => POST /path => JSON

    return client_.get(uri)
        ->expect_status(200, 201, 202, 204)
        ->expect_content_type("application/javascript")
        ->body(json_in)
        ->completion();
    json::parse(resp->body());

## PB => POST /path

