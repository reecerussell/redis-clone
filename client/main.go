package main

import (
	"context"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
)

func main() {
	now := time.Now()

	ctx := context.Background()
	rdb := redis.NewClient(&redis.Options{
		Addr:     "localhost:8573",
		Password: "", // no password set
		DB:       0,  // use default DB
		Protocol: 2,
	})
	defer rdb.Close()

	err := rdb.Set(ctx, "key", "value", 0).Err()
	if err != nil {
		panic(fmt.Errorf("failed to set: %w", err))
	}

	val, err := rdb.Get(ctx, "key").Result()
	if err != nil {
		panic(fmt.Errorf("failed to get: %w", err))
	}
	fmt.Println("key", val)

	val2, err := rdb.Get(ctx, "key2").Result()
	if err == redis.Nil {
		fmt.Println("key2 does not exist")
	} else if err != nil {
		panic(fmt.Errorf("failed to get key2: %w", err))
	} else {
		fmt.Println("key2", val2)
	}

	ms := time.Since(now).Microseconds()
	fmt.Printf("Completed in %dus\n", ms)
}
